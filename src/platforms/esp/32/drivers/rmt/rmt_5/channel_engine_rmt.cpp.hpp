/// @file channel_engine_rmt.cpp
/// @brief RMT5 ChannelEngine implementation
///
/// To enable RMT operational logging (channel creation, queueing,
/// transmission):
///   #define FASTLED_LOG_RMT_ENABLED
///
/// RMT logging uses FL_LOG_RMT which is compile-time controlled via fl/log.h.
/// When disabled (default), FL_LOG_RMT produces no code overhead (zero-cost
/// abstraction).

// Allow compilation on both ESP32 hardware and stub platforms (for testing)
#if defined(ESP32) || defined(FASTLED_STUB_IMPL)

#include "fl/compiler_control.h"

#ifdef ESP32
#include "platforms/esp/32/feature_flags/enabled.h"
#endif

// On ESP32: Check FASTLED_RMT5 flag
// On stub platforms: Always allow compilation (no feature flag check)
#if !defined(ESP32) || FASTLED_RMT5

#include "channel_engine_rmt.h"
#include "fl/chipsets/led_timing.h"
#include "fl/dbg.h"
#include "fl/delay.h"
#include "fl/error.h"
#include "fl/log.h"
#include "fl/pin.h"
#include "fl/warn.h"
#include "fl/stl/algorithm.h"
#include "fl/stl/bit_cast.h"
#include "fl/stl/assert.h"
#include "fl/stl/optional.h"
#include "fl/stl/sstream.h"
#include "fl/stl/time.h"
#include "fl/stl/unique_ptr.h"
#include "fl/trace.h"
#include "platforms/memory_barrier.h" // For FL_MEMORY_BARRIER

// Peripheral interface includes
#include "irmt5_peripheral.h"

// Platform-specific peripheral implementations
// Note: These headers define the concrete peripheral classes (ESP32 or Mock)
// and all necessary support types (RMT types, buffer pools, network detection).
// The #ifdef is isolated to these includes only - no conditional code below.
#ifndef FASTLED_STUB_IMPL
    // ESP32 platform: Real hardware drivers
    FL_EXTERN_C_BEGIN
    #include "driver/rmt_tx.h"  // For rmt_channel_handle_t, rmt_tx_done_event_data_t (callback signature)
    FL_EXTERN_C_END
    #include "rmt5_peripheral_esp.h"
    #include "common.h"  // ESP32 constants (FASTLED_RMT5_CLOCK_HZ, IRAM_ATTR, etc.)
    #include "rmt_memory_manager.h"  // RmtMemoryManager
    #include "buffer_pool.h"  // RMTBufferPool
    #include "network_detector.h"  // NetworkDetector
    #include "network_state_tracker.h"  // NetworkStateTracker
#else
    // Stub platform: Mock implementations for testing
    #include "platforms/shared/mock/esp/32/drivers/rmt5_peripheral_mock.h"
    #include "platforms/shared/mock/esp/32/drivers/rmt5_support_stubs.h"
#endif

namespace fl {

// Import types from detail namespace
using detail::IRMT5Peripheral;
using detail::Rmt5ChannelConfig;

// On ESP32: These types are in fl:: namespace
// On stub platforms: These types are in detail:: namespace
#ifdef FASTLED_STUB_IMPL
using detail::RmtMemoryManager;
using detail::NetworkDetector;
using detail::NetworkStateTracker;
using detail::RMTBufferPool;
#endif
// On ESP32, these types are already in fl:: namespace so no using needed

//=============================================================================
// Platform Peripheral Selection
//=============================================================================

/// @brief Get the default peripheral instance for the current platform
/// @return Reference to platform-specific peripheral (ESP32 or Mock)
///
/// This factory function hides the platform selection logic behind a single
/// function call. The #ifdef is isolated here rather than scattered throughout
/// the codebase.
static IRMT5Peripheral& getDefaultPeripheral() {
#ifdef FASTLED_STUB_IMPL
    return detail::Rmt5PeripheralMock::instance();
#else
    return detail::Rmt5PeripheralESP::instance();
#endif
}

//=============================================================================
// ChannelEngineRMTImpl - Implementation class
//=============================================================================
//
// NOTE: Rmt5EncoderImpl is now in rmt5_peripheral_esp.cpp (implementation detail).
// Encoders are created via mPeripheral.createEncoder() which returns opaque void*.
//=============================================================================

/**
 * @brief Implementation class for RMT5 Channel Engine
 *
 * All ESP-IDF types, state, and implementation details are kept in this
 * private implementation class. The public interface (ChannelEngineRMT)
 * remains clean and header-only.
 */
class ChannelEngineRMTImpl : public ChannelEngineRMT {
  public:
    // Production constructor: Use default platform peripheral
    ChannelEngineRMTImpl()
        : ChannelEngineRMTImpl(getDefaultPeripheral()) {
    }

    // Testing constructor: Inject peripheral
    explicit ChannelEngineRMTImpl(IRMT5Peripheral& peripheral)
        : mPeripheral(peripheral),
          mDMAChannelsInUse(0), mAllocationFailed(false),
          mMemoryReductionOffset(0),
          mConsecutiveAllocationFailures(0), mRecoveryWarningShown(false) {
        // Configure platform-specific logging (RMT and cache log levels)
        mPeripheral.configureLogging();

        FL_LOG_RMT("RMT Channel Engine initialized");
    }

    ~ChannelEngineRMTImpl() override {
        // Wait for all active transmissions to complete (with timeout)
        int timeout_iterations = 100000; // 10 seconds at 100us per iteration
        while (poll() == EngineState::BUSY && timeout_iterations > 0) {
            fl::delayMicroseconds(100);
            timeout_iterations--;
        }
        if (timeout_iterations == 0) {
            FL_WARN("ChannelEngineRMT destructor timeout - forcing cleanup");
        }

        // Get memory manager reference
        auto &memMgr = RmtMemoryManager::instance();

        // Destroy all channels
        for (auto &ch : mChannels) {
            if (ch.channel) {
                mPeripheral.waitAllDone(ch.channel, 1000);
                mPeripheral.disableChannel(ch.channel);
                mPeripheral.deleteChannel(ch.channel);

                // Restore GPIO pulldown when RMT channel is destroyed
                // This ensures pin returns to stable LOW state
                fl::pinMode(ch.pin, fl::PinMode::InputPulldown);

                // Free DMA if this channel was using it
                if (ch.useDMA) {
                    memMgr.freeDMA(ch.memoryChannelId,
                                   true); // true = TX channel
                    mDMAChannelsInUse--;
                }

                // Free memory from memory manager
                memMgr.free(ch.memoryChannelId, true); // true = TX channel
            }

            // Delete per-channel encoder
            if (ch.encoder) {
                mPeripheral.deleteEncoder(ch.encoder);
                ch.encoder = nullptr;
            }
        }
        mChannels.clear();

        FL_LOG_RMT("RMT Channel Engine destroyed");
    }

    // IChannelEngine interface implementation
    bool canHandle(const ChannelDataPtr& data) const override {
        if (!data) {
            return false;
        }
        // Clockless engines only handle non-SPI chipsets
        return !data->isSpi();
    }

    void enqueue(ChannelDataPtr channelData) override {
        mEnqueuedChannels.push_back(channelData);
    }

    void show() override {
        FL_SCOPED_TRACE;
        if (mEnqueuedChannels.empty()) {
            return;
        }
        FL_ASSERT(mTransmittingChannels.empty(),
                  "ChannelEngineRMT: Cannot show() while channels are still "
                  "transmitting");
        FL_ASSERT(poll() == EngineState::READY,
                  "ChannelEngineRMT: Cannot show() while hardware is busy");

        // Mark all channels as in use before transmission
        for (auto &channel : mEnqueuedChannels) {
            channel->setInUse(true);
        }

        // Create a const span from the enqueued channels and pass to
        // beginTransmission
        fl::span<const ChannelDataPtr> channelSpan(mEnqueuedChannels.data(),
                                                   mEnqueuedChannels.size());
        beginTransmission(channelSpan);

        // Move enqueued channels to transmitting list (async operation started)
        // Don't clear mInUse flags yet - poll() will do that when transmission
        // completes
        mTransmittingChannels = fl::move(mEnqueuedChannels);
    }

    EngineState poll() override {
        // Check hardware status
        bool anyActive = false;
        int activeCount = 0;
        int completedCount = 0;

        // Check each channel for completion
        for (auto &ch : mChannels) {
            if (!ch.inUse) {
                continue;
            }

            activeCount++;
            anyActive = true;

            if (ch.transmissionComplete) {
                completedCount++;
                FL_LOG_RMT("Channel on pin " << ch.pin
                                             << " completed transmission");

                // Disable channel to release HW resources
                if (ch.channel) {
                    bool disable_success = mPeripheral.disableChannel(ch.channel);
                    if (!disable_success) {
                        FL_LOG_RMT("Failed to disable channel");
                    }
                }

                // Release channel back to pool
                FL_LOG_RMT("Releasing channel " << ch.pin);
                releaseChannel(&ch);

                // Decrement activeCount since we just released this channel
                activeCount--;

                // Try to start pending channels
                processPendingChannels();
            } else {
                FL_LOG_RMT(
                    "Channel on pin "
                    << ch.pin
                    << " still transmitting (inUse=true, complete=false)");
            }
        }

        // Check if any pending channels remain
        if (!mPendingChannels.empty()) {
            anyActive = true;
            FL_LOG_RMT("Pending channels: " << mPendingChannels.size());
        } else if (activeCount > 0) {
            anyActive = true;
            FL_LOG_RMT("No pending channels, but "
                       << activeCount << " active channels (" << completedCount
                       << " completed)");
        } else {
            // No active channels and no pending channels
            anyActive = false;
        }

        // When transmission completes (READY or ERROR), clear the "in use"
        // flags and release the transmitted channels
        EngineState state = anyActive ? EngineState::BUSY : EngineState::READY;
        if (state == EngineState::READY) {
            for (auto &channel : mTransmittingChannels) {
                channel->setInUse(false);
            }
            mTransmittingChannels.clear();
        }

        return state;
    }

  private:
    /// @brief RMT channel state (replaces RmtWorkerSimple)
    struct ChannelState {
        void* channel;  // Opaque channel handle (matches IRMT5Peripheral interface)
        void* encoder;  // Encoder handle from peripheral interface (prevents race conditions)
        int pin;        // GPIO pin number (platform-agnostic)
        ChipsetTiming timing;
        volatile bool transmissionComplete;
        bool inUse;
        bool useDMA; // Whether this channel uses DMA
        uint32_t reset_us;
        fl::span<uint8_t> pooledBuffer; // Buffer acquired from pool (must be
                                        // released on complete)
        uint8_t memoryChannelId;        // Virtual channel ID for memory manager
                                        // accounting (vector index)
    };

    /// @brief Pending channel data to be transmitted when HW channels become
    /// available
    struct PendingChannel {
        ChannelDataPtr data;
        int pin;
        ChipsetTiming timing;
        uint32_t reset_us;
    };

    /// @brief Begin LED data transmission for all channels (internal)
    /// @param channelData Span of channel data to transmit
    void beginTransmission(fl::span<const ChannelDataPtr> channelData) {
        if (channelData.size() == 0) {
            FL_LOG_RMT("beginTransmission: No channels to transmit");
            return;
        }
        FL_LOG_RMT("ChannelEngineRMT::beginTransmission() is running");

        // Network-aware channel reconfiguration (once per frame)
        #if FASTLED_RMT_NETWORK_REDUCE_CHANNELS
            reconfigureForNetwork();
        #endif

        // Reset allocation failure flag at start of each frame to allow retry
        if (mAllocationFailed) {
            FL_LOG_RMT("Resetting allocation failure flag (retry at start of frame)");
            mAllocationFailed = false;
        }

        // Sort: smallest strips first (helps async parallelism)
        fl::vector_inlined<ChannelDataPtr, 16> sorted;
        for (const auto& data : channelData) {
            sorted.push_back(data);
        }
        fl::sort(sorted.begin(), sorted.end(), [](const ChannelDataPtr& a, const ChannelDataPtr& b) {
            return a->getSize() > b->getSize();  // Reverse order for back-to-front processing
        });

        // Queue all channels as pending first
        for (const auto& data : sorted) {
            int pin = data->getPin();
            const auto& timingCfg = data->getTiming();
            ChipsetTiming timing = {
                timingCfg.t1_ns,
                timingCfg.t2_ns,
                timingCfg.t3_ns,
                timingCfg.reset_us,
                timingCfg.name
            };
            mPendingChannels.push_back({data, pin, timing, timingCfg.reset_us});
        }

        // Start as many transmissions as HW channels allow
        processPendingChannels();
    }

    /// @brief Acquire a channel for given pin and timing
    /// @param dataSize Size of LED data in bytes (0 = use default buffer size)
    /// @return Pointer to channel state, or nullptr if no HW available
    ChannelState *acquireChannel(int pin, const ChipsetTiming &timing,
                                 fl::size dataSize = 0);

    /// @brief Release a channel (marks as available for reuse)
    void releaseChannel(ChannelState *channel);

    /// @brief Create new RMT channel with given configuration
    /// @param dataSize Size of LED data in bytes (0 = use default buffer size)
    /// @return true if channel created successfully
    bool createChannel(ChannelState *state, int pin,
                       const ChipsetTiming &timing, fl::size dataSize = 0) {
        // ============================================================================
        // RMT5 MEMORY MANAGEMENT - Now using centralized RmtMemoryManager
        // ============================================================================
        // Memory allocation policy:
        // - TX channels: Always double-buffer (2×
        // SOC_RMT_MEM_WORDS_PER_CHANNEL)
        // - DMA channels: Bypass on-chip memory (allocated from DRAM instead)
        // - RX channels: User-specified size (managed separately in
        // RmtRxChannel)
        //
        // The RmtMemoryManager tracks all allocations to prevent
        // over-allocation and coordinates memory usage between TX and RX
        // channels.
        // ============================================================================

        // Assign memory channel ID only for NEW channels (not during reconfiguration)
        // Check if this state already exists in mChannels vector by comparing addresses
        bool isExistingState = false;
        for (const auto& ch : mChannels) {
            if (&ch == state) {
                isExistingState = true;
                break;
            }
        }

        if (!isExistingState) {
            // New channel: assign next available ID (will be index after push_back)
            state->memoryChannelId = static_cast<uint8_t>(mChannels.size());
        }
        // else: Existing channel being reconfigured - keep existing memoryChannelId

        // Get memory manager reference
        auto &memMgr = RmtMemoryManager::instance();

        // Get current Network state for memory allocation
        bool networkActive = NetworkDetector::isAnyNetworkActive();

        // ============================================================================
        // DMA ALLOCATION POLICY - ESP32-S3 First Channel Only (TX or RX)
        // ============================================================================
        // ESP32-S3 has ONLY ONE RMT DMA channel (hardware limitation).
        // This DMA channel is SHARED between TX and RX channels.
        //
        // Allocation priority:
        //   1. FIRST channel created (TX or RX): Uses DMA (if data size > 0)
        //   2. ALL subsequent channels: Use non-DMA (on-chip double-buffer)
        //
        // DMA allocation is managed centrally by RmtMemoryManager to coordinate
        // between TX (ChannelEngineRMT) and RX (RmtRxChannel) drivers.
        // ============================================================================
        // Check with memory manager if DMA is available (handles platform detection)
        bool tryDMA = memMgr.isDMAAvailable();
        if (tryDMA) {
            // DMA slot available - first channel across TX/RX
            FL_LOG_RMT("TX Channel #" << (mChannels.size() + 1)
                                      << ": DMA slot available for pin "
                                      << static_cast<int>(pin)
                                      << " (data size: " << dataSize
                                      << " bytes)");
        } else {
            // DMA not available (platform doesn't support it, or slot is taken)
            FL_LOG_RMT("TX Channel #" << (mChannels.size() + 1)
                       << ": DMA not available, using non-DMA for pin "
                       << static_cast<int>(pin));
        }

        // STEP 1: Try DMA channel creation (first channel only on ESP32-S3)
        if (tryDMA && dataSize > 0) {
            // Allocate memory from memory manager (DMA bypasses on-chip memory)
            auto alloc_result = memMgr.allocateTx(
                state->memoryChannelId, true, networkActive); // true = use DMA
            if (!alloc_result.ok()) {
                FL_WARN("Memory manager TX allocation failed for DMA channel "
                        << static_cast<int>(state->memoryChannelId));
                return false;
            }

            // DMA mode: Use ESP-IDF recommended buffer size
            // ESP-IDF documentation recommends mem_block_symbols = 1024 for DMA mode
            // Reference: https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-reference/peripherals/rmt.html
            //
            // IMPORTANT: Unlike non-DMA mode which is limited by on-chip SRAM,
            // DMA mode streams data from DRAM. The mem_block_symbols parameter
            // controls DMA chunk size, not total capacity. Using too large a value
            // (e.g., dataSize * 8) causes ESP_ERR_INVALID_ARG from rmt_new_tx_channel().
            //
            // Related issues:
            // - https://github.com/espressif/esp-idf/issues/12564
            // - https://github.com/espressif/idf-extra-components/issues/466
            fl::size dma_mem_block_symbols = 1024;  // ESP-IDF recommended DMA buffer size
            FL_LOG_RMT("DMA allocation: "
                       << dma_mem_block_symbols << " symbols (DMA chunk size) for " << dataSize
                       << " bytes (" << (dataSize / 3) << " LEDs)");

            // RMT5 interrupt priority is always set to level 3 (highest
            // supported) RMT5 hardware limitation: Cannot boost priority above
            // level 3 Network-aware priority boost is not possible with RMT5
            int intr_priority = FL_RMT5_INTERRUPT_LEVEL; // Always level 3
            // #if FASTLED_RMT_NETWORK_PRIORITY_BOOST
            // if (networkActive) {
            //     intr_priority = FL_RMT5_INTERRUPT_LEVEL_NETWORK_MODE;
            //     FL_LOG_RMT("Network active: Boosting RMT interrupt priority
            //     to level " << intr_priority);
            // }
            // #endif

            Rmt5ChannelConfig dma_config(pin, FASTLED_RMT5_CLOCK_HZ,
                                          dma_mem_block_symbols, 1, true, intr_priority);

            bool dma_success =
                mPeripheral.createTxChannel(dma_config, (void**)&state->channel);
            if (dma_success) {
                // DMA SUCCESS - claim DMA slot in memory manager
                if (!memMgr.allocateDMA(state->memoryChannelId,
                                        true)) { // true = TX channel
                    FL_WARN("DMA hardware creation succeeded but memory "
                            "manager allocation failed");
                    mPeripheral.deleteChannel(state->channel);
                    state->channel = nullptr;
                    memMgr.free(state->memoryChannelId, true);
                    return false;
                }

                mDMAChannelsInUse++;
                state->pin = pin;
                state->timing = timing;
                state->useDMA = true;
                state->transmissionComplete = false;

                // Create encoder for this DMA channel
                state->encoder = mPeripheral.createEncoder(timing, FASTLED_RMT5_CLOCK_HZ);
                if (!state->encoder) {
                    FL_WARN("Failed to create encoder for DMA channel");
                    mPeripheral.deleteChannel(state->channel);
                    state->channel = nullptr;
                    mDMAChannelsInUse--;
                    // Free DMA and memory allocation
                    memMgr.freeDMA(state->memoryChannelId, true);
                    memMgr.free(state->memoryChannelId, true);
                    return false;
                }

                FL_LOG_RMT("✓ TX Channel #"
                           << (mChannels.size() + 1) << ": DMA enabled on GPIO "
                           << static_cast<int>(pin) << " ("
                           << dma_mem_block_symbols << " symbols)");
                return true;
            } else {
                // DMA FAILED - free memory and fall through to non-DMA
                // Free memory allocation
                memMgr.free(state->memoryChannelId, true);
                FL_WARN("DMA channel creation failed - unexpected failure on DMA-capable platform, falling back to non-DMA");
            }
        }

        // STEP 2: Create non-DMA channel (either DMA not attempted, failed, or
        // disabled) Allocate memory from memory manager (double-buffer policy)
        auto alloc_result = memMgr.allocateTx(state->memoryChannelId, false,
                                              networkActive); // false = non-DMA
        if (!alloc_result.ok()) {
            // Memory allocation failed - this can happen when:
            // 1. External RMT users (USB CDC, etc.) consume memory
            // 2. Too many non-DMA channels requested
            // 3. Network mode requires 3× buffering but insufficient memory
            //
            // Note: DMA channels consume 0 on-chip words, but ESP32-S3 only has
            // 1 DMA channel. Subsequent channels must use non-DMA (on-chip memory).
            FL_WARN("Memory manager TX allocation failed for channel "
                    << static_cast<int>(state->memoryChannelId)
                    << " - insufficient on-chip memory");
            FL_WARN("  Available: " << memMgr.availableTxWords() << " words");
            FL_WARN("  Requested: " << (RmtMemoryManager::calculateMemoryBlocks(networkActive) * SOC_RMT_MEM_WORDS_PER_CHANNEL) << " words");
            FL_WARN("  DMA channels in use: " << memMgr.getDMAChannelsInUse() << "/1");
            return false;
        }

        fl::size mem_block_symbols = alloc_result.value();

        // Apply previously discovered memory reduction offset (from self-healing)
        // This prevents re-running the progressive retry on every allocation
        if (mMemoryReductionOffset > 0 && mem_block_symbols > mMemoryReductionOffset) {
            FL_LOG_RMT("Applying memory reduction offset: " << mMemoryReductionOffset
                       << " symbols (learned from previous recovery)");
            mem_block_symbols -= mMemoryReductionOffset;
        }

        // Log channel number and allocation type
        size_t channel_num = mChannels.size() + 1;
        if (memMgr.getDMAChannelsInUse() > 0) {
            FL_LOG_RMT("✓ TX Channel #"
                       << channel_num
                       << ": Non-DMA (double-buffer: " << mem_block_symbols
                       << " words) - DMA slot taken by another channel");
        } else {
            FL_LOG_RMT("✓ TX Channel #"
                       << channel_num
                       << ": Non-DMA (double-buffer: " << mem_block_symbols
                       << " words) - No DMA support on platform");
        }

        // RMT5 interrupt priority is always set to level 3 (highest supported)
        // RMT5 hardware limitation: Cannot boost priority above level 3
        // Network-aware priority boost is not possible with RMT5
        int intr_priority = FL_RMT5_INTERRUPT_LEVEL; // Always level 3
        // #if FASTLED_RMT_NETWORK_PRIORITY_BOOST
        // if (networkActive) {
        //     intr_priority = FL_RMT5_INTERRUPT_LEVEL_NETWORK_MODE;
        //     FL_LOG_RMT("Network active: Boosting RMT interrupt priority to
        //     level " << intr_priority);
        // }
        // #endif

        Rmt5ChannelConfig tx_config(pin, FASTLED_RMT5_CLOCK_HZ,
                                     mem_block_symbols, 1, false, intr_priority);

        bool success = mPeripheral.createTxChannel(tx_config, (void**)&state->channel);

        // PROGRESSIVE RETRY MECHANISM FOR MEMORY ALLOCATION RECOVERY
        // =========================================================
        // If allocation fails: Actual RMT memory may be less than accounting expects
        // (external code using RMT peripherals, or hardware issues)
        //
        // Strategy: Progressively reduce mem_block_symbols and retry
        // - Start with full allocation (mem_block_symbols)
        // - Reduce by 1 symbol per retry until success or minimum reached
        // - Minimum: SOC_RMT_MEM_WORDS_PER_CHANNEL (1 block)

        if (!success && mem_block_symbols > SOC_RMT_MEM_WORDS_PER_CHANNEL) {
            FL_WARN("RMT channel allocation failed (initial request: "
                    << mem_block_symbols << " symbols)");
            FL_WARN("Attempting progressive memory reduction recovery...");

            mConsecutiveAllocationFailures++;
            size_t original_symbols = mem_block_symbols;
            size_t retry_count = 0;
            const size_t min_symbols = SOC_RMT_MEM_WORDS_PER_CHANNEL; // Minimum 1 block
            bool recovery_succeeded = false;

            // Progressively reduce memory until it works or we hit minimum
            for (size_t reduced_symbols = mem_block_symbols - 1;
                 reduced_symbols >= min_symbols;
                 reduced_symbols--) {
                retry_count++;

                // Free previous allocation attempt
                memMgr.free(state->memoryChannelId, true);

                // Try allocating with reduced memory (bypass memory manager, direct allocation)
                // We need to manually account for this since we're going below expected size
                Rmt5ChannelConfig retry_config(pin, FASTLED_RMT5_CLOCK_HZ,
                                                 reduced_symbols, 1, false, intr_priority);

                FL_LOG_RMT("Retry #" << retry_count << ": Attempting " << reduced_symbols
                           << " symbols (reduced by " << (original_symbols - reduced_symbols) << ")");

                success = mPeripheral.createTxChannel(retry_config, (void**)&state->channel);
                if (success) {
                    // SUCCESS - Record the memory reduction offset
                    mMemoryReductionOffset = original_symbols - reduced_symbols;
                    mConsecutiveAllocationFailures = 0; // Reset failure counter

                    // Calculate how many words we actually need to reserve
                    size_t external_words = original_symbols - reduced_symbols;

                    // Show recovery warning with user action guidance
                    if (!mRecoveryWarningShown) {
                        fl::sstream msg;
                        msg << "\n========================================\n"
                            << "RMT ALLOCATION RECOVERY - ACTION REQUIRED\n"
                            << "========================================\n"
                            << "FastLED detected external RMT memory usage!\n"
                            << "  Expected: " << original_symbols << " symbols available\n"
                            << "  Actual: " << reduced_symbols << " symbols available\n"
                            << "  External usage: ~" << external_words << " words\n"
                            << "\n"
                            << "To prevent future recovery attempts, add this to setup():\n"
                            << "----------------------------------------\n"
                            << "  auto& rmtMgr = fl::RmtMemoryManager::instance();\n"
                            << "  rmtMgr.reserveExternalMemory(" << external_words << ", 0);\n"
                            << "----------------------------------------\n"
                            << "\n"
                            << "FastLED will continue with reduced buffer size.\n"
                            << "Performance may be degraded during WiFi/network activity.\n"
                            << "========================================";
                        FL_WARN(msg.str());
                        mRecoveryWarningShown = true;
                    }

                    // Re-allocate through memory manager with reduced size
                    // (This ensures accounting stays synchronized)
                    mem_block_symbols = reduced_symbols;
                    recovery_succeeded = true;
                    break; // Exit retry loop
                }
            }

            // If recovery failed, show detailed error
            if (!recovery_succeeded) {
                fl::sstream msg;
                msg << "\n========================================\n"
                    << "RMT CHANNEL ALLOCATION FAILED\n"
                    << "========================================\n"
                    << "FastLED could not allocate RMT channel after " << retry_count << " retry attempts\n"
                    << "  Platform: " << CONFIG_IDF_TARGET << "\n"
                    << "  Requested: " << original_symbols << " symbols\n"
                    << "  Minimum attempted: " << min_symbols << " symbols\n"
                    << "\n"
                    << "Possible causes:\n"
                    << "  1. External code is using all RMT channels\n"
                    << "  2. RMT hardware failure\n"
                    << "  3. Insufficient RMT memory for platform\n"
                    << "\n"
                    << "LEDs on pin " << static_cast<int>(pin) << " will NOT work!\n"
                    << "========================================";
                FL_ERROR(msg.str());

                state->channel = nullptr;
                memMgr.free(state->memoryChannelId, true);
                return false;
            }

            // Recovery succeeded - fall through to encoder creation
            success = true;
        }

        if (!success) {
            // Non-recoverable error (already at minimum or other failure)
            FL_LOG_RMT("Failed to create non-DMA RMT channel on pin "
                       << static_cast<int>(pin));
            state->channel = nullptr;
            memMgr.free(state->memoryChannelId, true);
            return false;
        }

        // NOTE: Callback registration moved to registerChannelCallback()
        // to avoid dangling pointer issues when ChannelState is copied

        state->pin = pin;
        state->timing = timing;
        state->useDMA = false; // Non-DMA channel
        state->transmissionComplete = false;

        // Create encoder for this channel
        state->encoder = mPeripheral.createEncoder(timing, FASTLED_RMT5_CLOCK_HZ);
        if (!state->encoder) {
            FL_WARN("Failed to create encoder for channel");
            mPeripheral.deleteChannel(state->channel);
            state->channel = nullptr;
            // Free memory allocation
            memMgr.free(state->memoryChannelId, true);
            return false;
        }

        FL_LOG_RMT("Non-DMA RMT channel created on GPIO "
                   << static_cast<int>(pin) << " (" << mem_block_symbols
                   << " symbols) with dedicated encoder");
        return true;
    }

    /// @brief Register ISR callback for channel (must be called AFTER
    /// ChannelState is in final location)
    /// @return true if callback registered successfully
    bool registerChannelCallback(ChannelState *state);

    /// @brief Configure existing channel (handle pin/timing changes)
    /// @param dataSize Size of LED data in bytes (0 = use default buffer size)
    void configureChannel(ChannelState *state, int pin,
                          const ChipsetTiming &timing, fl::size dataSize = 0) {
        // Check if timing changed - if so, encoder must be recreated
        bool timingChanged =
            state->channel &&
            (state->timing.T1 != timing.T1 || state->timing.T2 != timing.T2 ||
             state->timing.T3 != timing.T3 ||
             state->timing.RESET != timing.RESET);

        // If pin changed, destroy and recreate channel
        if (state->channel && state->pin != pin) {
            FL_LOG_RMT("Pin changed from " << static_cast<int>(state->pin)
                                           << " to " << static_cast<int>(pin)
                                           << ", recreating channel");

            // Wait for any pending transmission to complete
            mPeripheral.waitAllDone(state->channel, 100);

            // Delete encoder before deleting channel
            if (state->encoder) {
                mPeripheral.deleteEncoder(state->encoder);
                state->encoder = nullptr;
            }

            // Delete channel directly - no need to disable since we're deleting
            // (rmt_del_channel handles cleanup internally)
            mPeripheral.deleteChannel(state->channel);
            state->channel = nullptr;

            // Free DMA and memory allocation
            auto &memMgr = RmtMemoryManager::instance();
            if (state->useDMA) {
                memMgr.freeDMA(state->memoryChannelId,
                               true); // true = TX channel
                mDMAChannelsInUse--;
            }
            memMgr.free(state->memoryChannelId, true);

            state->useDMA = false;
        }

        // If timing changed but channel exists, recreate encoder
        if (timingChanged) {
            FL_LOG_RMT("Timing changed for pin " << static_cast<int>(pin)
                                                 << ", recreating encoder");
            FL_LOG_RMT("  Old: T1=" << state->timing.T1
                                    << " T2=" << state->timing.T2
                                    << " T3=" << state->timing.T3);
            FL_LOG_RMT("  New: T1=" << timing.T1 << " T2=" << timing.T2
                                    << " T3=" << timing.T3);

            // Wait for any pending transmission to complete
            mPeripheral.waitAllDone(state->channel, 100);

            // Delete old encoder
            if (state->encoder) {
                mPeripheral.deleteEncoder(state->encoder);
                state->encoder = nullptr;
            }

            // Create new encoder with updated timing
            state->encoder = mPeripheral.createEncoder(timing, FASTLED_RMT5_CLOCK_HZ);
            if (!state->encoder) {
                FL_WARN("Failed to recreate encoder with new timing");
                // Channel is still valid but encoder is broken - mark channel
                // as unusable
                mPeripheral.deleteChannel(state->channel);
                state->channel = nullptr;

                // Free DMA and memory allocation
                auto &memMgr = RmtMemoryManager::instance();
                if (state->useDMA) {
                    memMgr.freeDMA(state->memoryChannelId, true);
                    mDMAChannelsInUse--;
                }
                memMgr.free(state->memoryChannelId, true);

                state->useDMA = false;
                return;
            }

            FL_LOG_RMT("Encoder recreated successfully with new timing");
        }

        // Create channel if needed
        if (!state->channel) {
            if (!createChannel(state, pin, timing, dataSize)) {
                FL_LOG_RMT("Failed to recreate channel for pin "
                           << static_cast<int>(pin));
                return;
            }

            // Register callback after creation (state pointer is already stable
            // here)
            if (!registerChannelCallback(state)) {
                FL_WARN("Failed to register callback after reconfiguration");
                if (state->encoder) {
                    mPeripheral.deleteEncoder(state->encoder);
                    state->encoder = nullptr;
                }
                if (state->useDMA) {
                    mDMAChannelsInUse--;
                }
                mPeripheral.deleteChannel(state->channel);
                state->channel = nullptr;

                // Free DMA and memory allocation
                auto &memMgr = RmtMemoryManager::instance();
                if (state->useDMA) {
                    memMgr.freeDMA(state->memoryChannelId,
                                   true); // true = TX channel
                    mDMAChannelsInUse--;
                }
                memMgr.free(state->memoryChannelId, true);

                state->useDMA = false;
                return;
            }
        }

        // Update timing
        state->timing = timing;
        state->transmissionComplete = false;
    }

    /// @brief Process pending channels that couldn't be started earlier
    void processPendingChannels() {
        if (mPendingChannels.empty()) {
            return;
        }

        // Try to start pending channels
        for (size_t i = 0; i < mPendingChannels.size(); ) {
            const auto& pending = mPendingChannels[i];

            // Get data size for DMA buffer calculation
            fl::size dataSize = pending.data->getSize();

            // Acquire channel for this transmission
            ChannelState* channel = acquireChannel(pending.pin, pending.timing, dataSize);
            if (!channel) {
                ++i;  // No HW available, leave in queue
                continue;
            }

            // Verify channel has encoder (should always be true if createChannel succeeded)
            if (!channel->encoder) {
                FL_WARN("Channel missing encoder for pin " << pending.pin);
                releaseChannel(channel);
                ++i;
                continue;
            }

            // Start transmission
            channel->reset_us = pending.reset_us;
            channel->transmissionComplete = false;

            // Acquire buffer from pool (PSRAM -> DRAM/DMA transfer)
            // Note: dataSize already retrieved earlier for channel acquisition
            fl::span<uint8_t> pooledBuffer = channel->useDMA ?
                mBufferPool.acquireDMA(dataSize) :
                mBufferPool.acquireInternal(dataSize);

            if (pooledBuffer.empty()) {
                FL_WARN("Failed to acquire pooled buffer for pin " << pending.pin
                        << " (" << dataSize << " bytes, DMA=" << channel->useDMA << ")");
                releaseChannel(channel);
                ++i;
                continue;
            }

            // Copy data from PSRAM to pooled buffer using writeWithPadding
            // Note: writeWithPadding uses zero padding since RMT can handle any byte array size
            pending.data->writeWithPadding(pooledBuffer);

            // Store pooled buffer in channel state for release on completion
            channel->pooledBuffer = pooledBuffer;

            bool enable_success = mPeripheral.enableChannel(channel->channel);
            if (!enable_success) {
                FL_LOG_RMT("Failed to enable channel");
                // Release buffer back to pool before releasing channel
                if (channel->useDMA) {
                    mBufferPool.releaseDMA();
                } else {
                    mBufferPool.releaseInternal(channel->pooledBuffer);
                }
                channel->pooledBuffer = fl::span<uint8_t>();
                releaseChannel(channel);
                ++i;
                continue;
            }

            // Explicitly reset encoder before each transmission to ensure clean state
            if (!mPeripheral.resetEncoder(channel->encoder)) {
                FL_LOG_RMT("Failed to reset encoder");
                mPeripheral.disableChannel(channel->channel);
                // Release buffer back to pool before releasing channel
                if (channel->useDMA) {
                    mBufferPool.releaseDMA();
                } else {
                    mBufferPool.releaseInternal(channel->pooledBuffer);
                }
                channel->pooledBuffer = fl::span<uint8_t>();
                releaseChannel(channel);
                ++i;
                continue;
            }

            // CRITICAL: Synchronize cache before DMA transmission
            // This ensures CPU writes to the LED buffer are flushed to SRAM
            // before RMT DMA reads the data. Without this, DMA may read stale
            // cache data, causing LED color corruption on ESP32-S3/C3/C6/H2.
            // NOTE: Only call syncCache() for DMA buffers.
            // Non-DMA buffers use internal SRAM (no cache coherency issues).
            if (channel->useDMA) {
                mPeripheral.syncCache(pooledBuffer.data(), pooledBuffer.size());
            }

            // Pass pooled buffer (DRAM/DMA) instead of PSRAM pointer
            bool tx_success = mPeripheral.transmit(channel->channel, channel->encoder,
                                                    pooledBuffer.data(),
                                                    pooledBuffer.size());
            if (!tx_success) {
                FL_LOG_RMT("Failed to transmit");
                mPeripheral.disableChannel(channel->channel);
                // Release buffer back to pool before releasing channel
                if (channel->useDMA) {
                    mBufferPool.releaseDMA();
                } else {
                    mBufferPool.releaseInternal(channel->pooledBuffer);
                }
                channel->pooledBuffer = fl::span<uint8_t>();
                releaseChannel(channel);
                ++i;
                continue;
            }

            FL_LOG_RMT("Started transmission for pin " << pending.pin
                       << " (" << pending.data->getSize() << " bytes)");

            // Remove from pending queue (swap with last and pop)
            if (i < mPendingChannels.size() - 1) {
                mPendingChannels[i] = mPendingChannels[mPendingChannels.size() - 1];
            }
            mPendingChannels.pop_back();
            // Don't increment i - we just moved a new element here
        }
    }

    /// @brief Destroy a single channel and free resources
    /// @param state Channel state to destroy (must not be in use)
    void destroyChannel(ChannelState *state);

    /// @brief Destroy least-used channels to free resources
    /// @param count Number of channels to destroy (from end of mChannels
    /// vector)
    void destroyLeastUsedChannels(size_t count);

    /// @brief Calculate target channel count based on network state and
    /// platform
    /// @param networkActive Whether any network (WiFi, Ethernet, or Bluetooth)
    /// is currently active
    /// @return Target number of channels for current state
    size_t calculateTargetChannelCount(bool networkActive) {
        if (!networkActive) {
            // No network: Use maximum channels for platform (2× memory blocks)
            #if defined(CONFIG_IDF_TARGET_ESP32)
                return 4;  // 512 words ÷ 128 = 4 channels
            #elif defined(CONFIG_IDF_TARGET_ESP32S2)
                return 2;  // 256 words ÷ 128 = 2 channels
            #elif defined(CONFIG_IDF_TARGET_ESP32S3)
                return 3;  // 1 DMA + 2 on-chip (192 ÷ 96 = 2)
            #elif defined(CONFIG_IDF_TARGET_ESP32C3) || defined(CONFIG_IDF_TARGET_ESP32C6) || \
                  defined(CONFIG_IDF_TARGET_ESP32H2) || defined(CONFIG_IDF_TARGET_ESP32C5)
                return 1;  // C3/C6/H2/C5: Only 96 words
            #else
                return 1;  // Unknown platform: conservative default
            #endif
        } else {
            // Network active: Reduce channels to allow 3× buffering (except C3/C6/H2/C5)
            #if defined(CONFIG_IDF_TARGET_ESP32)
                return 2;  // 512 words ÷ 192 = 2 channels (3× buffering)
            #elif defined(CONFIG_IDF_TARGET_ESP32S2)
                return 1;  // 256 words ÷ 192 = 1 channel (3× buffering)
            #elif defined(CONFIG_IDF_TARGET_ESP32S3)
                return 2;  // 1 DMA + 1 on-chip (192 words ÷ 144 = 1 on-chip with 3×)
            #elif defined(CONFIG_IDF_TARGET_ESP32C3) || defined(CONFIG_IDF_TARGET_ESP32C6) || \
                  defined(CONFIG_IDF_TARGET_ESP32H2) || defined(CONFIG_IDF_TARGET_ESP32C5)
                return 1;  // C3/C6/H2/C5: Cannot use 3× (insufficient memory), keep 1 channel
            #else
                return 1;  // Unknown platform: conservative default
            #endif
        }
    }

    /// @brief Reconfigure channels for network state change (destroy/recreate
    /// as needed)
    void reconfigureForNetwork() {
        // Check if Network state changed using singleton tracker
        auto& networkTracker = NetworkStateTracker::instance();
        if (!networkTracker.hasChanged()) {
            return;  // No change - nothing to do
        }

        bool networkActive = networkTracker.isActive();
        bool wasNetworkActive = !networkActive;  // Previous state is opposite of current
        FL_DBG("Network state changed: " << (networkActive ? "ACTIVE" : "INACTIVE")
               << " (was: " << (wasNetworkActive ? "ACTIVE" : "INACTIVE") << ")");

        // Check if memory allocation size would actually change
        // On platforms like ESP32-C3/C6/H2 with limited memory, network state doesn't affect memory blocks
        auto& memMgr = RmtMemoryManager::instance();
        size_t oldMemBlocks = RmtMemoryManager::calculateMemoryBlocks(wasNetworkActive);
        size_t newMemBlocks = RmtMemoryManager::calculateMemoryBlocks(networkActive);

        if (oldMemBlocks == newMemBlocks) {
            FL_DBG("Network state changed but memory allocation size unchanged ("
                   << newMemBlocks << "× blocks) - skipping reconfiguration");
            return;  // Memory allocation size doesn't change - no need to reconfigure
        }

        FL_DBG("Memory allocation changing from " << oldMemBlocks << "× to "
               << newMemBlocks << "× blocks due to network state change");

        // Calculate target channel count for new Network state
        size_t targetChannels = calculateTargetChannelCount(networkActive);
        FL_DBG("Target channel count: " << targetChannels << " (current: " << mChannels.size() << ")");

        // PHASE 1: Destroy excess channels if Network activated and we exceed target
        if (networkActive && mChannels.size() > targetChannels) {
            size_t channelsToDestroy = mChannels.size() - targetChannels;
            FL_DBG("Network activated - destroying " << channelsToDestroy << " excess channels");
            destroyLeastUsedChannels(channelsToDestroy);
        }

        // PHASE 2: Reconfigure remaining idle channels with new memory allocation
        // This ensures existing channels use Network-appropriate memory (2× vs 3× blocks)
        size_t reconfigured = 0;

        for (size_t i = 0; i < mChannels.size(); ++i) {
            ChannelState& state = mChannels[i];

            // Skip channels that are in use or already destroyed
            if (state.inUse || !state.channel) {
                continue;
            }

            // Destroy the channel and its encoder
            FL_DBG("Reconfiguring idle channel " << i << " (pin: " << static_cast<int>(state.pin) << ")");

            // Delete encoder first
            if (state.encoder) {
                mPeripheral.deleteEncoder(state.encoder);
                state.encoder = nullptr;
            }

            // Delete channel
            if (state.channel) {
                mPeripheral.waitAllDone(state.channel, 100);
                mPeripheral.deleteChannel(state.channel);
                state.channel = nullptr;
            }

            // Free DMA if this channel was using it
            if (state.useDMA) {
                memMgr.freeDMA(state.memoryChannelId, true);  // true = TX channel
                mDMAChannelsInUse--;
                state.useDMA = false;
            }

            // Free memory allocation
            memMgr.free(state.memoryChannelId, true);  // true = TX channel

            // Recreate channel with Network-appropriate memory allocation
            // Note: createChannel() will call memMgr.allocateTx() with current Network state
            if (createChannel(&state, state.pin, state.timing, 0)) {
                // Re-register callback for the recreated channel
                if (!registerChannelCallback(&state)) {
                    FL_WARN("Failed to re-register callback for reconfigured channel " << i);
                    // Channel creation succeeded but callback failed - destroy it
                    if (state.channel) {
                        mPeripheral.deleteChannel(state.channel);
                        state.channel = nullptr;
                    }
                    if (state.encoder) {
                        mPeripheral.deleteEncoder(state.encoder);
                        state.encoder = nullptr;
                    }
                    // Free memory allocation that createChannel() made
                    if (state.useDMA) {
                        memMgr.freeDMA(state.memoryChannelId, true);
                        mDMAChannelsInUse--;
                        state.useDMA = false;
                    }
                    memMgr.free(state.memoryChannelId, true);
                } else {
                    reconfigured++;
                    FL_DBG("Successfully reconfigured channel " << i);
                }
            } else {
                FL_WARN("Failed to recreate channel " << i << " during Network reconfiguration");
            }
        }

        FL_DBG("Network reconfiguration complete - " << reconfigured << " channels reconfigured");
    }

    /// @brief ISR callback for transmission completion
    static bool IRAM_ATTR transmitDoneCallback(
        void* channel, const void* edata,
        void* user_data);

    /// @brief Peripheral interface (real or mock)
    IRMT5Peripheral& mPeripheral;

    /// @brief All RMT channels (active and idle)
    fl::vector_inlined<ChannelState, 16> mChannels;

    /// @brief Pending channel data waiting for show() to be called
    fl::vector_inlined<ChannelDataPtr, 16> mEnqueuedChannels;

    /// @brief Pending channels waiting for available HW (after show() called)
    fl::vector_inlined<PendingChannel, 16> mPendingChannels;

    /// @brief Channels currently being transmitted (for cleanup on poll())
    fl::vector_inlined<ChannelDataPtr, 16> mTransmittingChannels;

    /// @brief Buffer pool for PSRAM -> DRAM/DMA memory transfer
    RMTBufferPool mBufferPool;

    /// @brief Track DMA channel usage
    ///
    /// ESP32-S3 Hardware Limitation: Only 1 RMT DMA channel available
    /// - mDMAChannelsInUse == 0: DMA available (first channel)
    /// - mDMAChannelsInUse >= 1: DMA exhausted (all subsequent channels use
    /// non-DMA)
    ///
    /// This counter is incremented when a DMA channel is successfully created
    /// and decremented when a DMA channel is destroyed.
    int mDMAChannelsInUse;

    /// @brief Track allocation failures to avoid hammering the driver
    bool mAllocationFailed;

    /// @brief Progressive retry: Memory reduction offset (symbols to reduce per retry)
    /// When RMT allocation fails, progressively reduce memory allocation by this amount
    size_t mMemoryReductionOffset;

    /// @brief Track consecutive allocation failures for progressive retry
    size_t mConsecutiveAllocationFailures;

    /// @brief Track whether recovery warning has been shown to user
    bool mRecoveryWarningShown;
};

//=============================================================================
// Internal Transmission Logic
//=============================================================================


//=============================================================================
// Channel Management
//=============================================================================

ChannelEngineRMTImpl::ChannelState *ChannelEngineRMTImpl::acquireChannel(
    int pin, const ChipsetTiming &timing, fl::size dataSize) {
    // Strategy 1: Find channel with matching pin (zero-cost reuse)
    // This applies to both DMA and non-DMA channels
    FL_LOG_RMT("acquireChannel: Finding channel with matching pin "
               << static_cast<int>(pin));
    for (auto &ch : mChannels) {
        if (!ch.inUse && ch.channel && ch.pin == pin) {
            ch.inUse = true;
            configureChannel(&ch, pin, timing, dataSize);
            FL_LOG_RMT("Reusing " << (ch.useDMA ? "DMA" : "non-DMA")
                                  << " channel for pin "
                                  << static_cast<int>(pin));
            return &ch;
        }
    }

    // Strategy 2: Find any idle non-DMA channel (requires reconfiguration)
    for (auto &ch : mChannels) {
        if (!ch.inUse && ch.channel && !ch.useDMA) {
            ch.inUse = true;
            configureChannel(&ch, pin, timing, dataSize);
            FL_LOG_RMT("Reconfiguring idle non-DMA channel for pin "
                       << static_cast<int>(pin));
            return &ch;
        }
    }

    // Strategy 3: Create new channel if HW available
    // BUT: Skip if allocation previously failed (reset at start of next frame
    // in beginTransmission)
    if (mAllocationFailed) {
        FL_LOG_RMT("Skipping channel creation (allocation failed, will retry "
                   "next frame)");
        return nullptr;
    }

    ChannelState newCh = {};
    if (createChannel(&newCh, pin, timing, dataSize)) {
        newCh.inUse = true;
        mChannels.push_back(fl::move(newCh));

        // CRITICAL: Register callback AFTER pushing to vector to ensure stable
        // pointer
        ChannelState *stablePtr = &mChannels.back();
        if (!registerChannelCallback(stablePtr)) {
            FL_WARN("Failed to register callback for new channel");
            // Free memory allocation that createChannel() made
            auto &memMgr = RmtMemoryManager::instance();
            if (stablePtr->useDMA) {
                memMgr.freeDMA(stablePtr->memoryChannelId, true);
                mDMAChannelsInUse--;
            }
            memMgr.free(stablePtr->memoryChannelId, true);
            mPeripheral.deleteChannel(stablePtr->channel);
            mChannels.pop_back();
            mAllocationFailed = true; // Mark failure
            return nullptr;
        }

        FL_LOG_RMT("Created new channel for pin "
                   << static_cast<int>(pin) << " (total: " << mChannels.size()
                   << ")");
        return stablePtr;
    }

    // No HW channels available - mark allocation failed
    FL_LOG_RMT("Channel allocation failed - max channels reached");
    mAllocationFailed = true;
    return nullptr;
}

void ChannelEngineRMTImpl::releaseChannel(ChannelState *channel) {
    FL_ASSERT(channel != nullptr, "releaseChannel called with nullptr");

    // CRITICAL: Wait for RMT hardware to fully complete transmission
    // The ISR callback (transmitDoneCallback) may fire slightly before the last
    // bits have fully propagated out of the RMT peripheral. We must ensure
    // hardware is truly done before allowing buffer reuse to prevent race
    // conditions.
    bool wait_success = mPeripheral.waitAllDone(channel->channel, 100);
    FL_ASSERT(wait_success,
              "RMT transmission wait timeout - hardware may be stalled");

    // Release pooled buffer if one was acquired
    if (!channel->pooledBuffer.empty()) {
        if (channel->useDMA) {
            mBufferPool.releaseDMA();
        } else {
            mBufferPool.releaseInternal(channel->pooledBuffer);
        }
        channel->pooledBuffer = fl::span<uint8_t>();
    }

    channel->inUse = false;
    channel->transmissionComplete = false;

    // Restore GPIO pulldown when RMT releases pin
    // This ensures pin returns to stable LOW state between transmissions
    fl::pinMode(channel->pin, fl::PinMode::InputPulldown);

    // NOTE: Keep channel and encoder alive for reuse
}

bool ChannelEngineRMTImpl::registerChannelCallback(ChannelState *state) {
    FL_ASSERT(state != nullptr, "registerChannelCallback called with nullptr");
    FL_ASSERT(state->channel != nullptr,
              "registerChannelCallback called with null channel");

    // Register transmission completion callback
    // CRITICAL: state pointer must be stable (not on stack, not subject to
    // vector reallocation)
    bool success = mPeripheral.registerTxCallback(
        state->channel,
        &transmitDoneCallback,
        state);
    if (!success) {
        FL_WARN("Failed to register callbacks");
        return false;
    }

    FL_LOG_RMT("Registered callback for channel on GPIO "
               << static_cast<int>(state->pin));
    return true;
}

//=============================================================================
// Network-Aware Channel Destruction Helpers
//=============================================================================

void ChannelEngineRMTImpl::destroyChannel(ChannelState *state) {
    FL_ASSERT(state != nullptr, "destroyChannel called with nullptr");

    if (!state->channel) {
        return; // Already destroyed
    }

    // Wait for transmission to complete (should already be done if !inUse)
    bool wait_success = mPeripheral.waitAllDone(state->channel, 100);
    if (!wait_success) {
        FL_WARN("destroyChannel: Wait all done timeout for pin "
                << static_cast<int>(state->pin));
    }

    // Delete encoder
    if (state->encoder) {
        mPeripheral.deleteEncoder(state->encoder);
        state->encoder = nullptr;
    }

    // Delete channel
    bool del_success = mPeripheral.deleteChannel(state->channel);
    if (!del_success) {
        FL_WARN("destroyChannel: Failed to delete channel");
    }
    state->channel = nullptr;

    // Free memory resources
    auto &memMgr = RmtMemoryManager::instance();
    if (state->useDMA) {
        memMgr.freeDMA(state->memoryChannelId, true); // true = TX channel
        mDMAChannelsInUse--;
    }
    memMgr.free(state->memoryChannelId, true); // true = TX channel

    state->useDMA = false;

    FL_LOG_RMT("Destroyed channel on pin "
               << static_cast<int>(state->pin) << " (memoryChannelId: "
               << static_cast<int>(state->memoryChannelId) << ")");
}

void ChannelEngineRMTImpl::destroyLeastUsedChannels(size_t count) {
    if (count == 0) {
        return;
    }

    FL_LOG_RMT("Destroying " << count << " least-used channels");

    // Destroy channels from end of vector (FIFO - oldest channels at end)
    // NOTE: Future enhancement could track lastUsedTimestamp for true LRU
    // behavior
    size_t destroyed = 0;
    while (destroyed < count && !mChannels.empty()) {
        auto &state = mChannels.back();

        // Only destroy if not in use
        if (!state.inUse) {
            destroyChannel(&state);
            mChannels.pop_back();
            destroyed++;
        } else {
            // Cannot destroy in-use channel - skip for now
            // This could happen if WiFi activates during transmission
            FL_WARN("destroyLeastUsedChannels: Cannot destroy in-use channel "
                    "on pin "
                    << static_cast<int>(state.pin) << ", skipping");
            break;
        }
    }

    FL_LOG_RMT("Destroyed " << destroyed << " channels (requested: " << count
                            << ")");
}

//=============================================================================
// ISR Callback
//=============================================================================

/// @brief ISR callback fired when RMT transmission completes
///
/// CRITICAL TIMING CONTRACT:
/// This callback is invoked by ESP-IDF when the RMT peripheral signals that
/// the transmission queue is empty. However, there is a small timing window
/// where the callback may fire BEFORE the last bits have fully propagated
/// out of the RMT shift register and onto the GPIO pin.
///
/// RACE CONDITION PREVENTION:
/// To prevent buffer corruption, releaseChannel() MUST call
/// rmt_tx_wait_all_done() with a timeout BEFORE marking the channel as
/// available for reuse. This ensures the RMT hardware has fully completed
/// transmission before:
///   1. The channel's pooled buffer is released back to the buffer pool
///   2. The ChannelData's mInUse flag is cleared (allowing new pixel data
///   writes)
///   3. The channel is marked as available for acquisition by other
///   transmissions
///
/// Without this hardware wait, the following race condition can occur:
///   - Frame N transmission starts (RMT hardware reading buffer A)
///   - ISR fires (callback sets transmissionComplete = true)
///   - Main thread calls releaseChannel() → clears mInUse flag
///   - User calls FastLED.show() for Frame N+1
///   - ClocklessRMT::showPixels() sees mInUse == false → proceeds to encode
///   - NEW pixel data overwrites buffer A while RMT is still shifting it out
///   - LEDs display corrupted mix of Frame N and Frame N+1 data
///
/// SYNCHRONIZATION STRATEGY:
/// - ISR: Sets transmissionComplete flag (lightweight, non-blocking)
/// - Main thread poll(): Calls releaseChannel() when transmissionComplete is
/// true
/// - releaseChannel(): Calls rmt_tx_wait_all_done() to ensure hardware is done
/// - ClocklessRMT::showPixels(): Asserts !mInUse before writing new pixel data
///
/// This multi-layered approach provides both correctness (hardware wait) and
/// fail-fast debugging (assertions catch any timing bugs).
bool IRAM_ATTR ChannelEngineRMTImpl::transmitDoneCallback(
    void* channel, const void* edata,
    void* user_data) {
    // Note: edata is rmt_tx_done_event_data_t* on ESP32, but we don't use it
    ChannelState *state = static_cast<ChannelState *>(user_data);
    if (!state) {
        // ISR callback received null user_data - this is a bug
        return false;
    }

    // Mark transmission as complete (polled by main thread)
    // NOTE: This flag triggers releaseChannel(), which performs hardware wait
    state->transmissionComplete = true;

    // Non-blocking design - no semaphore signal needed
    return false; // No task switch needed
}

//=============================================================================
// Factory Method Implementation
//=============================================================================

fl::shared_ptr<ChannelEngineRMT> ChannelEngineRMT::create() {
    return fl::make_shared<ChannelEngineRMTImpl>();
}


} // namespace fl

#endif // FASTLED_RMT5 or FASTLED_STUB_IMPL

#endif // ESP32 or FASTLED_STUB_IMPL
