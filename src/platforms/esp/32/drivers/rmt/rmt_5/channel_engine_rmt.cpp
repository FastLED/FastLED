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

#ifdef ESP32

#include "fl/compiler_control.h"
#include "platforms/esp/32/feature_flags/enabled.h"

#if FASTLED_RMT5

#include "channel_engine_rmt.h"
#include "buffer_pool.h"
#include "common.h"
#include "fl/chipsets/led_timing.h"
#include "fl/dbg.h"
#include "fl/delay.h"
#include "fl/log.h"
#include "fl/warn.h"
#include "ftl/algorithm.h"
#include "ftl/assert.h"
#include "ftl/optional.h"
#include "ftl/time.h"
#include "ftl/unique_ptr.h"
#include "network_detector.h"
#include "rmt_memory_manager.h"

FL_EXTERN_C_BEGIN
#include "driver/gpio.h"
#include "driver/rmt_tx.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h" // For pdMS_TO_TICKS
FL_EXTERN_C_END

namespace fl {

//=============================================================================
// Rmt5EncoderImpl - RMT5 encoder implementation (inlined from
// rmt5_encoder.h/cpp)
//=============================================================================

/**
 * RMT5 Encoder Implementation - Plain struct for converting pixel bytes to RMT
 * symbols
 *
 * Architecture:
 * - Plain struct (no inheritance, no virtuals) for standard layout guarantee
 * - Uses ESP-IDF's official encoder pattern (rmt_encoder_t)
 * - Combines bytes_encoder (for pixel data) + copy_encoder (for reset pulse)
 * - State machine handles partial encoding when buffer fills
 * - Runs in ISR context - must be fast and ISR-safe
 * - rmt_encoder_t is first member to enable clean casting without offsetof
 *
 * Implementation based on ESP-IDF led_strip example:
 * https://github.com/espressif/esp-idf/tree/master/examples/peripherals/rmt/led_strip
 */
struct Rmt5EncoderImpl {
    // CRITICAL: rmt_encoder_t MUST be first member for clean casting
    // This allows: rmt_encoder_t* -> Rmt5EncoderImpl* via reinterpret_cast
    rmt_encoder_t base;

    // Sub-encoders
    rmt_encoder_handle_t mBytesEncoder;
    rmt_encoder_handle_t mCopyEncoder;

    // Encoder state machine counter (0 = data, 1 = reset)
    int mState;

    // Reset pulse symbol (low signal for RESET microseconds)
    rmt_symbol_word_t mResetCode;

    // Timing configuration (stored for debugging)
    uint32_t mBit0HighTicks;
    uint32_t mBit0LowTicks;
    uint32_t mBit1HighTicks;
    uint32_t mBit1LowTicks;
    uint32_t mResetTicks;

    // Factory method to create encoder instance
    static Rmt5EncoderImpl *create(const ChipsetTiming &timing,
                                   uint32_t resolution_hz) {
        Rmt5EncoderImpl *impl = new Rmt5EncoderImpl(timing, resolution_hz);
        if (impl == nullptr) {
            FL_WARN("Rmt5EncoderImpl::create: Failed to allocate encoder");
            return nullptr;
        }
        if (impl->getHandle() == nullptr) {
            FL_WARN("Rmt5EncoderImpl::create: Encoder initialization failed");
            delete impl;
            return nullptr;
        }
        return impl;
    }

    // Reinitialize encoder with new timing configuration
    esp_err_t reinit(const ChipsetTiming &timing, uint32_t resolution_hz) {
        if (mBytesEncoder) {
            rmt_del_encoder(mBytesEncoder);
            mBytesEncoder = nullptr;
        }
        if (mCopyEncoder) {
            rmt_del_encoder(mCopyEncoder);
            mCopyEncoder = nullptr;
        }
        return initialize(timing, resolution_hz);
    }

    // Get the underlying encoder handle for RMT transmission
    rmt_encoder_handle_t getHandle() { return &base; }

    // Destructor - cleans up sub-encoders
    ~Rmt5EncoderImpl() { cleanup(); }

    // Non-copyable
    Rmt5EncoderImpl(const Rmt5EncoderImpl &) = delete;
    Rmt5EncoderImpl &operator=(const Rmt5EncoderImpl &) = delete;

  private:
    // Private constructor - use create() factory method
    Rmt5EncoderImpl(const ChipsetTiming &timing, uint32_t resolution_hz)
        : mBytesEncoder(nullptr), mCopyEncoder(nullptr), mState(0),
          mBit0HighTicks(0), mBit0LowTicks(0), mBit1HighTicks(0),
          mBit1LowTicks(0), mResetTicks(0) {
        base.encode = Rmt5EncoderImpl::encodeCallback;
        base.reset = Rmt5EncoderImpl::resetCallback;
        base.del = Rmt5EncoderImpl::delCallback;

        mResetCode.duration0 = 0;
        mResetCode.level0 = 0;
        mResetCode.duration1 = 0;
        mResetCode.level1 = 0;

        esp_err_t ret = initialize(timing, resolution_hz);
        if (ret != ESP_OK) {
            FL_WARN("Rmt5EncoderImpl: Initialization failed: "
                    << esp_err_to_name(ret));
        }
    }

    // Private methods
    size_t FL_IRAM encode(rmt_channel_handle_t channel,
                          const void *primary_data, size_t data_size,
                          rmt_encode_state_t *ret_state) {
        rmt_encode_state_t session_state = RMT_ENCODING_RESET;
        rmt_encode_state_t state = RMT_ENCODING_RESET;
        size_t encoded_symbols = 0;

        switch (mState) {
        case 0:
            encoded_symbols +=
                mBytesEncoder->encode(mBytesEncoder, channel, primary_data,
                                      data_size, &session_state);
            if (session_state & RMT_ENCODING_COMPLETE) {
                mState = 1;
            }
            if (session_state & RMT_ENCODING_MEM_FULL) {
                state = static_cast<rmt_encode_state_t>(state |
                                                        RMT_ENCODING_MEM_FULL);
                goto out;
            }
            [[fallthrough]];

        case 1:
            encoded_symbols +=
                mCopyEncoder->encode(mCopyEncoder, channel, &mResetCode,
                                     sizeof(mResetCode), &session_state);
            if (session_state & RMT_ENCODING_COMPLETE) {
                mState = 0;
                state = static_cast<rmt_encode_state_t>(state |
                                                        RMT_ENCODING_COMPLETE);
            }
            if (session_state & RMT_ENCODING_MEM_FULL) {
                state = static_cast<rmt_encode_state_t>(state |
                                                        RMT_ENCODING_MEM_FULL);
                goto out;
            }
            break;

        default:
            state = RMT_ENCODING_RESET;
            break;
        }

    out:
        *ret_state = state;
        return encoded_symbols;
    }

    esp_err_t FL_IRAM reset() {
        mState = 0;
        if (mBytesEncoder) {
            mBytesEncoder->reset(mBytesEncoder);
        }
        if (mCopyEncoder) {
            mCopyEncoder->reset(mCopyEncoder);
        }
        return ESP_OK;
    }

    esp_err_t cleanup() {
        if (mBytesEncoder) {
            rmt_del_encoder(mBytesEncoder);
            mBytesEncoder = nullptr;
        }
        if (mCopyEncoder) {
            rmt_del_encoder(mCopyEncoder);
            mCopyEncoder = nullptr;
        }
        return ESP_OK;
    }

    esp_err_t initialize(const ChipsetTiming &timing, uint32_t resolution_hz) {
        const uint64_t ns_per_tick = 1000000000ULL / resolution_hz;

        mBit0HighTicks =
            static_cast<uint32_t>((timing.T1 + ns_per_tick / 2) / ns_per_tick);
        mBit0LowTicks =
            static_cast<uint32_t>((timing.T2 + ns_per_tick / 2) / ns_per_tick);
        mBit1HighTicks =
            static_cast<uint32_t>((timing.T2 + ns_per_tick / 2) / ns_per_tick);
        mBit1LowTicks =
            static_cast<uint32_t>((timing.T3 + ns_per_tick / 2) / ns_per_tick);
        mResetTicks = static_cast<uint32_t>(
            (timing.RESET * 1000ULL + ns_per_tick / 2) / ns_per_tick);

        rmt_bytes_encoder_config_t bytes_config = {};
        bytes_config.bit0.level0 = 1;
        bytes_config.bit0.duration0 = mBit0HighTicks;
        bytes_config.bit0.level1 = 0;
        bytes_config.bit0.duration1 = mBit0LowTicks;
        bytes_config.bit1.level0 = 1;
        bytes_config.bit1.duration0 = mBit1HighTicks;
        bytes_config.bit1.level1 = 0;
        bytes_config.bit1.duration1 = mBit1LowTicks;
        bytes_config.flags.msb_first = 0;

        esp_err_t ret = rmt_new_bytes_encoder(&bytes_config, &mBytesEncoder);
        if (ret != ESP_OK) {
            return ret;
        }

        rmt_copy_encoder_config_t copy_config = {};
        ret = rmt_new_copy_encoder(&copy_config, &mCopyEncoder);
        if (ret != ESP_OK) {
            rmt_del_encoder(mBytesEncoder);
            mBytesEncoder = nullptr;
            return ret;
        }

        mResetCode.duration0 = mResetTicks;
        mResetCode.level0 = 0;
        mResetCode.duration1 = 0;
        mResetCode.level1 = 0;

        return ESP_OK;
    }

    // Static callbacks for rmt_encoder_t interface
    static size_t FL_IRAM encodeCallback(rmt_encoder_t *encoder,
                                         rmt_channel_handle_t channel,
                                         const void *primary_data,
                                         size_t data_size,
                                         rmt_encode_state_t *ret_state) {
        Rmt5EncoderImpl *impl = reinterpret_cast<Rmt5EncoderImpl *>(encoder);
        return impl->encode(channel, primary_data, data_size, ret_state);
    }

    static esp_err_t FL_IRAM resetCallback(rmt_encoder_t *encoder) {
        Rmt5EncoderImpl *impl = reinterpret_cast<Rmt5EncoderImpl *>(encoder);
        return impl->reset();
    }

    static esp_err_t delCallback(rmt_encoder_t *encoder) {
        Rmt5EncoderImpl *impl = reinterpret_cast<Rmt5EncoderImpl *>(encoder);
        delete impl;
        return ESP_OK;
    }
};

//=============================================================================
// ChannelEngineRMTImpl - Implementation class with all ESP-IDF details
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
    ChannelEngineRMTImpl()
        : mDMAChannelsInUse(0), mAllocationFailed(false),
          mLastKnownNetworkState(false) {
        // Suppress ESP-IDF RMT "no free channels" errors (expected during
        // time-multiplexing) Only show critical RMT errors (ESP_LOG_ERROR and
        // above)
        esp_log_level_set("rmt", ESP_LOG_NONE);

        FL_LOG_RMT("RMT Channel Engine initialized");
    }

    ~ChannelEngineRMTImpl() override {
        // Wait for all active transmissions to complete
        while (poll() == EngineState::BUSY) {
            fl::delayMicroseconds(100);
        }

        // Get memory manager reference
        auto &memMgr = RmtMemoryManager::instance();

        // Destroy all channels
        for (auto &ch : mChannels) {
            if (ch.channel) {
                rmt_tx_wait_all_done(ch.channel, pdMS_TO_TICKS(1000));
                rmt_disable(ch.channel);
                rmt_del_channel(ch.channel);

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
            ch.encoder.reset();
        }
        mChannels.clear();

        FL_LOG_RMT("RMT Channel Engine destroyed");
    }

    // IChannelEngine interface implementation
    void enqueue(ChannelDataPtr channelData) override {
        mEnqueuedChannels.push_back(channelData);
    }

    void show() override {
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
                    esp_err_t err = rmt_disable(ch.channel);
                    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
                        FL_LOG_RMT("Failed to disable channel: " << err);
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
        rmt_channel_handle_t channel;
        fl::unique_ptr<Rmt5EncoderImpl> encoder; // Per-channel encoder (prevents race conditions)
        gpio_num_t pin;
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
            bool networkActive = NetworkDetector::isAnyNetworkActive();
            reconfigureForNetwork(networkActive);
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
    ChannelState *acquireChannel(gpio_num_t pin, const ChipsetTiming &timing,
                                 fl::size dataSize = 0);

    /// @brief Release a channel (marks as available for reuse)
    void releaseChannel(ChannelState *channel);

    /// @brief Create new RMT channel with given configuration
    /// @param dataSize Size of LED data in bytes (0 = use default buffer size)
    /// @return true if channel created successfully
    bool createChannel(ChannelState *state, gpio_num_t pin,
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

        // Set memory channel ID (will be the index in mChannels vector after
        // push_back)
        state->memoryChannelId = static_cast<uint8_t>(mChannels.size());

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

            // DMA mode: allocate enough symbols for the entire LED strip
            // Each byte needs 8 RMT symbols (1 symbol per bit)
            // Add extra space for reset pulse (typically 1 symbol, but use 16
            // for safety)
            fl::size dma_mem_block_symbols = (dataSize * 8) + 16;
            FL_LOG_RMT("DMA allocation: "
                       << dma_mem_block_symbols << " symbols for " << dataSize
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

            rmt_tx_channel_config_t dma_config = {};
            dma_config.gpio_num = pin;
            dma_config.clk_src = RMT_CLK_SRC_DEFAULT;
            dma_config.resolution_hz = FASTLED_RMT5_CLOCK_HZ;
            dma_config.mem_block_symbols = dma_mem_block_symbols;
            dma_config.trans_queue_depth = 1;
            dma_config.flags.invert_out = 0;
            dma_config.flags.with_dma = 1; // Enable DMA
            dma_config.intr_priority = intr_priority;

            esp_err_t dma_err =
                rmt_new_tx_channel(&dma_config, &state->channel);
            if (dma_err == ESP_OK) {
                // DMA SUCCESS - claim DMA slot in memory manager
                if (!memMgr.allocateDMA(state->memoryChannelId,
                                        true)) { // true = TX channel
                    FL_WARN("DMA hardware creation succeeded but memory "
                            "manager allocation failed");
                    rmt_del_channel(state->channel);
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
                state->encoder.reset(Rmt5EncoderImpl::create(timing, FASTLED_RMT5_CLOCK_HZ));
                if (!state->encoder) {
                    FL_WARN("Failed to create encoder for DMA channel");
                    state->encoder.reset();
                    rmt_del_channel(state->channel);
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
                FL_WARN("DMA channel creation failed: "
                        << esp_err_to_name(dma_err)
                        << " - unexpected failure on DMA-capable platform, "
                           "falling back to non-DMA");
            }
        }

        // STEP 2: Create non-DMA channel (either DMA not attempted, failed, or
        // disabled) Allocate memory from memory manager (double-buffer policy)
        auto alloc_result = memMgr.allocateTx(state->memoryChannelId, false,
                                              networkActive); // false = non-DMA
        if (!alloc_result.ok()) {
            FL_WARN("Memory manager TX allocation failed for channel "
                    << static_cast<int>(state->memoryChannelId)
                    << " - insufficient on-chip memory");
            return false;
        }

        fl::size mem_block_symbols = alloc_result.value();

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

        rmt_tx_channel_config_t tx_config = {};
        tx_config.gpio_num = pin;
        tx_config.clk_src = RMT_CLK_SRC_DEFAULT;
        tx_config.resolution_hz = FASTLED_RMT5_CLOCK_HZ;
        tx_config.mem_block_symbols = mem_block_symbols;
        tx_config.trans_queue_depth = 1;
        tx_config.flags.invert_out = 0;
        tx_config.flags.with_dma = 0; // Non-DMA
        tx_config.intr_priority = intr_priority;

        esp_err_t err = rmt_new_tx_channel(&tx_config, &state->channel);
        if (err != ESP_OK) {
            // This is expected when all HW channels are in use
            // (time-multiplexing scenario)
            FL_LOG_RMT("Failed to create non-DMA RMT channel on pin "
                       << static_cast<int>(pin) << ": "
                       << esp_err_to_name(err));
            state->channel = nullptr;
            // Free memory allocation
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
        state->encoder.reset(Rmt5EncoderImpl::create(timing, FASTLED_RMT5_CLOCK_HZ));
        if (!state->encoder) {
            FL_WARN("Failed to create encoder for channel");
            state->encoder.reset();
            rmt_del_channel(state->channel);
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
    void configureChannel(ChannelState *state, gpio_num_t pin,
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
            rmt_tx_wait_all_done(state->channel, pdMS_TO_TICKS(100));

            // Delete encoder before deleting channel
            state->encoder.reset();

            // Delete channel directly - no need to disable since we're deleting
            // (rmt_del_channel handles cleanup internally)
            rmt_del_channel(state->channel);
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
            rmt_tx_wait_all_done(state->channel, pdMS_TO_TICKS(100));

            // Delete old encoder
            state->encoder.reset();

            // Create new encoder with updated timing
            state->encoder.reset(Rmt5EncoderImpl::create(timing, FASTLED_RMT5_CLOCK_HZ));
            if (!state->encoder) {
                FL_WARN("Failed to recreate encoder with new timing");
                state->encoder.reset();
                // Channel is still valid but encoder is broken - mark channel
                // as unusable
                rmt_del_channel(state->channel);
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
                state->encoder.reset();
                if (state->useDMA) {
                    mDMAChannelsInUse--;
                }
                rmt_del_channel(state->channel);
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
            ChannelState* channel = acquireChannel(static_cast<gpio_num_t>(pending.pin), pending.timing, dataSize);
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

            esp_err_t err = rmt_enable(channel->channel);
            if (err != ESP_OK) {
                FL_LOG_RMT("Failed to enable channel: " << esp_err_to_name(err));
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
            rmt_encoder_handle_t enc_handle = channel->encoder->getHandle();
            err = enc_handle->reset(enc_handle);
            if (err != ESP_OK) {
                FL_LOG_RMT("Failed to reset encoder: " << esp_err_to_name(err));
                rmt_disable(channel->channel);
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

            rmt_transmit_config_t tx_config = {};
            tx_config.loop_count = 0;
            // Pass pooled buffer (DRAM/DMA) instead of PSRAM pointer
            err = rmt_transmit(channel->channel, channel->encoder->getHandle(),
                              pooledBuffer.data(),
                              pooledBuffer.size(),
                              &tx_config);
            if (err != ESP_OK) {
                FL_LOG_RMT("Failed to transmit: " << esp_err_to_name(err));
                rmt_disable(channel->channel);
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
    /// @param networkActive Whether any network (WiFi, Ethernet, or Bluetooth)
    /// is currently active
    void reconfigureForNetwork(bool networkActive) {
        // Check if Network state changed
        if (networkActive == mLastKnownNetworkState) {
            return;  // No change - nothing to do
        }

        FL_DBG("Network state changed: " << (networkActive ? "ACTIVE" : "INACTIVE")
               << " (was: " << (mLastKnownNetworkState ? "ACTIVE" : "INACTIVE") << ")");

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
        auto& memMgr = RmtMemoryManager::instance();
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
            state.encoder.reset();

            // Delete channel
            if (state.channel) {
                rmt_tx_wait_all_done(state.channel, pdMS_TO_TICKS(100));
                rmt_del_channel(state.channel);
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
                        rmt_del_channel(state.channel);
                        state.channel = nullptr;
                    }
                    state.encoder.reset();
                } else {
                    reconfigured++;
                    FL_DBG("Successfully reconfigured channel " << i);
                }
            } else {
                FL_WARN("Failed to recreate channel " << i << " during Network reconfiguration");
            }
        }

        // Update last known state
        mLastKnownNetworkState = networkActive;
        FL_DBG("Network reconfiguration complete - " << reconfigured << " channels reconfigured");
    }

    /// @brief ISR callback for transmission completion
    static bool IRAM_ATTR transmitDoneCallback(
        rmt_channel_handle_t channel, const rmt_tx_done_event_data_t *edata,
        void *user_data);

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

    /// @brief Track last known network state for change detection
    bool mLastKnownNetworkState;
};

//=============================================================================
// Internal Transmission Logic
//=============================================================================


//=============================================================================
// Channel Management
//=============================================================================

ChannelEngineRMTImpl::ChannelState *ChannelEngineRMTImpl::acquireChannel(
    gpio_num_t pin, const ChipsetTiming &timing, fl::size dataSize) {
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
            if (stablePtr->useDMA) {
                mDMAChannelsInUse--;
            }
            rmt_del_channel(stablePtr->channel);
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
    esp_err_t wait_result =
        rmt_tx_wait_all_done(channel->channel, pdMS_TO_TICKS(100));
    FL_ASSERT(wait_result == ESP_OK,
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
    // NOTE: Keep channel and encoder alive for reuse
}

bool ChannelEngineRMTImpl::registerChannelCallback(ChannelState *state) {
    FL_ASSERT(state != nullptr, "registerChannelCallback called with nullptr");
    FL_ASSERT(state->channel != nullptr,
              "registerChannelCallback called with null channel");

    // Register transmission completion callback
    // CRITICAL: state pointer must be stable (not on stack, not subject to
    // vector reallocation)
    rmt_tx_event_callbacks_t cbs = {};
    cbs.on_trans_done = transmitDoneCallback;
    esp_err_t err =
        rmt_tx_register_event_callbacks(state->channel, &cbs, state);
    if (err != ESP_OK) {
        FL_WARN("Failed to register callbacks: " << esp_err_to_name(err));
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
    esp_err_t wait_result =
        rmt_tx_wait_all_done(state->channel, pdMS_TO_TICKS(100));
    if (wait_result != ESP_OK) {
        FL_WARN("destroyChannel: rmt_tx_wait_all_done timeout for pin "
                << static_cast<int>(state->pin));
    }

    // Delete encoder
    state->encoder.reset();

    // Delete channel
    esp_err_t del_err = rmt_del_channel(state->channel);
    if (del_err != ESP_OK) {
        FL_WARN("destroyChannel: Failed to delete channel: "
                << esp_err_to_name(del_err));
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
    rmt_channel_handle_t channel, const rmt_tx_done_event_data_t *edata,
    void *user_data) {
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

#endif // FASTLED_RMT5

#endif // ESP32
