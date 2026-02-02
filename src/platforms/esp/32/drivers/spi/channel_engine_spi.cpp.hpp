/// @file channel_engine_spi.cpp
/// @brief Clockless-over-SPI ChannelEngine implementation for ESP32
///
/// ⚠️ ARCHITECTURE NOTE: This is NOT a general SPI LED driver!
/// This driver implements CLOCKLESS protocols (WS2812, SK6812, etc.) using SPI hardware
/// as a bit-banging engine. The SPI clock is used internally for timing but is NEVER
/// physically connected to the LED strip - only the MOSI/data pin is used.
/// See channel_engine_spi.h for detailed explanation.
///
/// @todo Known issues and future improvements:
/// - [x] ESP32-S3 CACHE COHERENCE: Fixed by using non-aligned heap_caps_malloc()
///       which forces the SPI driver to copy data internally. This copy flushes
///       CPU cache to physical memory, solving the DMA cache coherence issue.
///       Trade-off: ~50% memory overhead for internal copy buffer.
/// - [ ] Multi-SPI host support: Currently limited by single-host-per-bus design.

#ifdef ESP32

#include "platforms/esp/32/feature_flags/enabled.h"

#if FASTLED_ESP32_HAS_CLOCKLESS_SPI

#include "channel_engine_spi.h"
#include "platforms/esp/32/drivers/spi/wave8_encoder_spi.h" // wave8 encoding (need after main header)
#include "fl/dbg.h"
#include "fl/delay.h"
#include "fl/math_macros.h"
#include "fl/warn.h"
#include "fl/stl/time.h"
#include "fl/channels/detail/wave8.hpp" // Inline wave8 functions
#include "platforms/esp/32/drivers/spi/spi_hw_base.h" // SPI host definitions (SPI2_HOST, SPI3_HOST)
#include "platforms/esp/is_esp.h" // Platform detection (FL_IS_ESP_32C6, etc.)
#include "platforms/memory_barrier.h" // FL_MEMORY_BARRIER for cache coherence

FL_EXTERN_C_BEGIN
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_check.h"
#include "esp_heap_caps.h"
#include "soc/spi_periph.h"
#include "soc/gpio_sig_map.h"  // GPIO matrix signal IDs (FSPID_OUT_IDX, HSPID_OUT_IDX, etc.)
#include "esp_rom_gpio.h"  // esp_rom_gpio_connect_out_signal for manual GPIO routing
#if __has_include("esp_cache.h")
#include "esp_cache.h"  // For esp_cache_msync (ESP32-C6, ESP32-S3, etc.)
#define FASTLED_SPI_HAS_CACHE_API 1
#else
#define FASTLED_SPI_HAS_CACHE_API 0
#endif
#if __has_include("esp_dma_utils.h")
#include "esp_dma_utils.h"  // For esp_dma_capable_malloc (ESP-IDF 5.x)
#define FASTLED_SPI_HAS_DMA_UTILS 1
#else
#define FASTLED_SPI_HAS_DMA_UTILS 0
#endif
FL_EXTERN_C_END

namespace fl {

// Static member initialization
vector_inlined<ChannelEngineSpi::SpiHostTracking, 3>
    ChannelEngineSpi::sSpiHostUsage;

namespace {
/// @brief Calculate greatest common divisor using Euclidean algorithm
constexpr uint32_t gcd(uint32_t a, uint32_t b) {
    while (b != 0) {
        uint32_t temp = b;
        b = a % b;
        a = temp;
    }
    return a;
}

/// @brief Calculate GCD of four values
constexpr uint32_t gcd4(uint32_t a, uint32_t b, uint32_t c, uint32_t d) {
    return gcd(gcd(gcd(a, b), c), d);
}

/// @brief Count number of set bits in a value
constexpr uint8_t countBits(uint32_t value) {
    uint8_t count = 0;
    while (value) {
        count++;
        value >>= 1;
    }
    return count;
}
} // namespace

ChannelEngineSpi::ChannelEngineSpi()
    : mAllocationFailed(false), mLastRetryFrame(0) {
    FL_DBG("ChannelEngineSpi: Constructor called");
}

ChannelEngineSpi::~ChannelEngineSpi() {
    FL_DBG("ChannelEngineSpi: Destructor called");

    // Poll to ensure cleanup of any in-flight transmissions
    poll();

    mMultiLaneConfigs.clear();

    // Clean up all channels
    for (auto &channel : mChannels) {
        // Detach timer ISR
        if (channel.timerHandle.is_valid()) {
            fl::isr::detachHandler(channel.timerHandle);
        }

        // Free staging buffers (allocated with heap_caps_aligned_alloc)
        if (channel.stagingA) {
            heap_caps_free(channel.stagingA);  // heap_caps_aligned_alloc uses heap_caps_free()
        }
        if (channel.stagingB) {
            heap_caps_free(channel.stagingB);
        }
        // Free LED source buffer
        if (channel.ledSourceBuffer) {
            heap_caps_free(channel.ledSourceBuffer);
        }

        // Remove SPI device
        if (channel.spi_device) {
            // Wait for any pending transactions
            if (channel.transAInFlight || channel.transBInFlight) {
                spi_transaction_t *trans_ptr;
                spi_device_get_trans_result(channel.spi_device, &trans_ptr,
                                            pdMS_TO_TICKS(1000));
                if (channel.transAInFlight || channel.transBInFlight) {
                    spi_device_get_trans_result(channel.spi_device, &trans_ptr,
                                                pdMS_TO_TICKS(1000));
                }
            }
            spi_bus_remove_device(channel.spi_device);
        }

        // Release SPI host
        if (channel.spi_host != SPI_HOST_MAX) {
            releaseSpiHost(channel.spi_host);
        }
    }
}

bool ChannelEngineSpi::canHandle(const ChannelDataPtr& data) const {
    if (!data) {
        return false;
    }

    // ⚠️ ARCHITECTURE CLARIFICATION: This is a CLOCKLESS-over-SPI engine!
    //
    // This engine uses SPI hardware to implement CLOCKLESS LED protocols (WS2812, SK6812, etc.),
    // NOT true SPI protocols (APA102, SK9822, etc.). The SPI clock pin is used internally for
    // precise timing generation but is NEVER physically connected to the LED strip - only the
    // MOSI/data pin carries signals to the LEDs.
    //
    // How it works:
    //   - Clockless LED bits are encoded as SPI bit patterns (e.g., 100b = '0', 110b = '1')
    //   - The SPI clock controls MOSI timing (e.g., 2.5MHz for WS2812 = 400ns per bit)
    //   - LEDs decode pulse widths on the data line, ignoring the clock signal
    //
    // ✅ CORRECTED LOGIC:
    // Accept CLOCKLESS chipsets (WS2812, SK6812), reject TRUE SPI chipsets (APA102, SK9822)
    // True SPI chipsets should route to SpiChannelEngineAdapter (priority 5-9), not this engine
    //
    // Correct routing:
    //   APA102 → SpiChannelEngineAdapter (true SPI hardware)
    //   WS2812 → ChannelEngineSpi (this engine, clockless-over-SPI)
    return !data->isSpi();  // ✅ FIXED: Accept clockless, reject true SPI
}

void ChannelEngineSpi::configureMultiLanePins(
    const MultiLanePinConfig &pinConfig) {
    if (pinConfig.data0_pin < 0) {
        FL_WARN("ChannelEngineSpi: Invalid multi-lane config - data0_pin must "
                "be >= 0");
        return;
    }

    uint8_t laneCount = pinConfig.getLaneCount();
    FL_DBG("ChannelEngineSpi: Configuring "
           << static_cast<int>(laneCount) << "-lane SPI for pin "
           << pinConfig.data0_pin << " (data0=" << pinConfig.data0_pin
           << ", data1=" << pinConfig.data1_pin << ", data2="
           << pinConfig.data2_pin << ", data3=" << pinConfig.data3_pin << ")");

// Validate platform capabilities
#if defined(FL_IS_ESP_32C6) || defined(FL_IS_ESP_32C3) ||                      \
    defined(FL_IS_ESP_32H2)
    // ESP32-C6/C3/H2: Dual-lane max (no quad support)
    if (laneCount > 2) {
        FL_WARN("ChannelEngineSpi: ESP32-C6/C3/H2 only supports dual-lane SPI "
                "(max 2 lanes), "
                << "requested " << static_cast<int>(laneCount) << " lanes");
        return;
    }
#endif

    // Store the configuration
    mMultiLaneConfigs[pinConfig.data0_pin] = pinConfig;
    FL_DBG("ChannelEngineSpi: Multi-lane configuration stored for pin "
           << pinConfig.data0_pin);
}

void ChannelEngineSpi::enqueue(ChannelDataPtr channelData) {
    if (!channelData) {
        FL_WARN("ChannelEngineSpi: Null channel data passed to enqueue()");
        return;
    }

    mEnqueuedChannels.push_back(channelData);
}

void ChannelEngineSpi::show() {
    if (mEnqueuedChannels.empty()) {
        return;
    }

    // Move enqueued channels to transmitting list
    mTransmittingChannels = fl::move(mEnqueuedChannels);
    mEnqueuedChannels.clear();

    // Begin batched transmission (groups by timing, batches when N > K)
    beginBatchedTransmission(mTransmittingChannels);
}

IChannelEngine::EngineState ChannelEngineSpi::poll() {
    bool anyBusy = false;
    bool anyError = false;

    // Check all active channels
    for (auto &channel : mChannels) {
        if (!channel.inUse)
            continue;

        // With blocking transmit (Iteration 5 cache fix), transmission is complete
        // as soon as beginTransmission() returns. Check transmissionComplete flag.
        if (channel.transmissionComplete) {
            // Clear in-use flag on source data
            if (channel.sourceData) {
                channel.sourceData->setInUse(false);
            }

            releaseChannel(&channel);
        } else {
            // Still waiting for transmission to complete (shouldn't happen with blocking)
            anyBusy = true;
        }
    }

    // Process pending channels if hardware available
    if (!mPendingChannels.empty() && !anyBusy) {
        processPendingChannels();
    }

    // Determine final state
    if (anyError)
        return EngineState::ERROR;
    if (anyBusy)
        return EngineState::BUSY;
    if (!mPendingChannels.empty())
        return EngineState::DRAINING;

    // All channels complete - clear transmitting list
    if (!mTransmittingChannels.empty()) {
        mTransmittingChannels.clear();
    }

    return EngineState::READY;
}

void ChannelEngineSpi::beginBatchedTransmission(
    fl::span<const ChannelDataPtr> channels) {

    // Safety check: Pending queue should be empty before starting new batches
    // If pending channels exist, it indicates incomplete transmission from previous frame
    // or hardware saturation, which would interfere with batching logic.
    if (!mPendingChannels.empty()) {
        FL_WARN_EVERY(100, "ChannelEngineSpi: Pending queue not empty at batch start ("
                << mPendingChannels.size() << " channels pending). "
                << "This may indicate hardware saturation or incomplete previous frame.");
    }

    // ============================================================================
    // PHASE 1: Group channels by timing compatibility
    // ============================================================================
    // Why: SPI hardware requires all lanes to use the same clock rate and timing.
    //      We can only batch channels with identical timing requirements together.
    //
    // Example: If you have 4 WS2812 strips and 4 SK6812 strips, they form 2 groups:
    //   - Group 1 (WS2812): 4 channels with 2.5MHz clock, 100/110 bit patterns
    //   - Group 2 (SK6812): 4 channels with different clock/patterns
    //
    // Hash map key: SpiTimingConfig (clock_hz, bits_per_led_bit, protocol, reset_time_us)
    // Hash map value: Vector of channels with matching timing
    fl::hash_map<SpiTimingConfig, fl::vector<ChannelDataPtr>, TimingHash, TimingEqual> timingGroups;

    for (const auto& channel : channels) {
        SpiTimingConfig timing = getSpiTimingFromChannel(channel);
        timingGroups[timing].push_back(channel);
    }

    FL_DBG_EVERY(100, "ChannelEngineSpi: Grouped " << channels.size()
           << " channels into " << timingGroups.size() << " timing groups");

    // ============================================================================
    // PHASE 2: Process each timing group with batching
    // ============================================================================
    // Why: Each timing group can be batched independently. Batches within a group
    //      transmit sequentially (Batch 1, then Batch 2, etc.).
    //
    // Example: 8 WS2812 channels, 4-lane hardware capacity:
    //   - N = 8 (total channels in group)
    //   - K = 4 (available hardware lanes)
    //   - numBatches = ceil(8/4) = 2
    //   - Batch 1: Channels 0-3 (4 channels)
    //   - Batch 2: Channels 4-7 (4 channels)
    //
    // Performance: Without batching = 8 sequential transmissions
    //              With batching = 2 sequential batches (4x speedup potential)
    for (const auto& [timing, groupChannels] : timingGroups) {
        // Determine lane capacity (K) for this timing group
        // K = max number of channels that can transmit simultaneously
        uint8_t K = determineLaneCapacity(groupChannels);
        size_t N = groupChannels.size();
        size_t numBatches = (N + K - 1) / K;  // ceil(N/K)

        FL_DBG_EVERY(100, "ChannelEngineSpi: Timing group with " << N << " channels, "
               << static_cast<int>(K) << " lanes → " << numBatches << " batches");

        // ========================================================================
        // PHASE 3: Transmit each batch sequentially (blocking)
        // ========================================================================
        // Why: Batches must complete before the next batch starts to avoid
        //      hardware conflicts and maintain data integrity.
        //
        // Blocking behavior: show() remains blocking as expected by users.
        //   - Start batch transmission
        //   - Poll until READY (all channels in batch complete)
        //   - Move to next batch
        //
        // State transitions per batch:
        //   READY → beginTransmission() → BUSY → DRAINING → READY
        for (size_t batchIdx = 0; batchIdx < numBatches; batchIdx++) {
            size_t batchStart = batchIdx * K;
            size_t batchEnd = fl_min(batchStart + K, N);


            // Create span for this batch (subset of groupChannels)
            fl::span<const ChannelDataPtr> batch(
                groupChannels.data() + batchStart,
                batchEnd - batchStart
            );

            // Start batch transmission (calls beginTransmission for this batch)
            beginTransmission(batch);

            // Wait for batch to complete (blocking)
            // Must wait for READY state (not just BUSY):
            //   - BUSY: Channels actively transmitting
            //   - DRAINING: Pending channels waiting for hardware
            //   - READY: All channels complete, hardware idle
            //
            // Critical: Early versions only checked BUSY, missing DRAINING state.
            //           This caused batches to overlap incorrectly.
            EngineState state;
            while ((state = poll()) != EngineState::READY) {
                if (state == EngineState::ERROR) {
                    FL_WARN_EVERY(10, "ChannelEngineSpi: Error during batch transmission");
                    break;
                }
                // Yield CPU to allow ISR timer and other tasks to run (prevents LED flickering)
                taskYIELD();
            }

            // Insert reset delay between batches (critical for LED protocol compliance)
            // LEDs require a reset pulse (low signal) to latch data and prepare for next frame.
            // Without this delay, LEDs interpret the next batch as frame continuation,
            // causing alternating black/color frames (protocol violation).
            if (batchIdx + 1 < numBatches) {
                FL_DBG_EVERY(100, "ChannelEngineSpi: Inserting reset delay ("
                       << timing.reset_time_us << " μs) between batches");
                fl::delayMicroseconds(timing.reset_time_us);
            }
        }
    }
}

uint8_t ChannelEngineSpi::determineLaneCapacity(
    fl::span<const ChannelDataPtr> channels) {

    // Determine the maximum number of channels that can transmit in parallel (K).
    //
    // ESP32 SPI Hardware Limitation:
    //   - Each SPI host (SPI2_HOST, SPI3_HOST) can only have ONE bus configuration
    //   - Each host uses one set of pins, so only ONE channel per host
    //   - See acquireSpiHost() at line 756: "if (tracking->refCount == 0)"
    //
    // Platform Capacity:
    //   - ESP32/S2/S3/P4: 2 SPI hosts → K=2 (parallel transmission)
    //   - ESP32-C3: 1 SPI host → K=1 (sequential transmission)
    //   - (SPI1_HOST exists but is flash-reserved, unreliable for LEDs)
    //
    // By returning the actual SPI host count, we enable:
    //   - If N <= K: Single batch, all channels submit together (hardware queues excess)
    //   - If N > K: Multiple batches, but minimal batching overhead
    //
    // Note: The pending queue mechanism (mPendingChannels) handles excess channels
    // when hardware is saturated, so we want K to match actual parallel capacity,
    // not the theoretical maximum.

// POLICY: Use only ONE SPI host at a time for reliability.
// Multi-host usage (SPI2 + SPI3 simultaneously) can cause resource conflicts
// and makes debugging difficult. Sequential transmission on a single host
// is more reliable and simplifies the driver logic.
//
// Note: Multi-lane SPI (dual/quad mode) can still be used on a SINGLE host
// for parallel data lines to multiple LED strips, but that's different from
// using multiple SPI peripheral hosts.
    constexpr uint8_t PARALLEL_SPI_HOSTS = 1;  // Single SPI host policy

    FL_DBG_EVERY(100, "ChannelEngineSpi: Determined lane capacity: "
           << static_cast<int>(PARALLEL_SPI_HOSTS) << " SPI hosts");
    return PARALLEL_SPI_HOSTS;
}

void ChannelEngineSpi::beginTransmission(
    fl::span<const ChannelDataPtr> channelData) {
    for (const auto &data : channelData) {
        gpio_num_t pin = static_cast<gpio_num_t>(data->getPin());

        // Get timing configuration from channel data
        SpiTimingConfig timing = getSpiTimingFromChannel(data);

        // Get original chipset timing for wave8 LUT precision
        const ChipsetTimingConfig& originalTiming = data->getTiming();

        // DEBUG: Trace timing values received from channel data
        FL_WARN_EVERY(100, "ChannelEngineSpi: Received timing from ChannelData: t1_ns="
               << originalTiming.t1_ns << ", t2_ns=" << originalTiming.t2_ns
               << ", t3_ns=" << originalTiming.t3_ns << ", reset_us=" << originalTiming.reset_us);

        // Get LED data from channel
        const auto &ledData = data->getData();
        if (ledData.empty()) {
            FL_WARN("ChannelEngineSpi: Empty LED data for pin " << pin);
            continue;
        }

        // Acquire or reuse channel
        SpiChannelState *channel = acquireChannel(pin, timing, ledData.size(), originalTiming);
        if (!channel) {
            // No hardware available - queue for later
            FL_DBG("ChannelEngineSpi: No HW available for pin " << pin
                                                                << ", queuing");
            mPendingChannels.push_back({data, pin, timing});
            continue;
        }

        // Mark data as in-use (prevents strip driver from modifying during TX)
        data->setInUse(true);

        // Initialize streaming state for this channel
        channel->stagingOffset = 0;
        channel->currentStaging = channel->stagingA;
        channel->transAInFlight = false;
        channel->transBInFlight = false;
        channel->transmissionComplete = false;

        // CRITICAL: Copy LED data to internal SRAM buffer for ISR-safe access
        // PSRAM is NOT safe for ISR access and will return zeros/stale data
        if (channel->ledSourceBuffer && channel->ledSourceBufferSize >= ledData.size()) {
            // Copy to internal SRAM buffer
            fl::memcpy(channel->ledSourceBuffer, ledData.data(), ledData.size());
            channel->ledSource = channel->ledSourceBuffer;
            FL_DBG_EVERY(100, "ChannelEngineSpi: Copied " << ledData.size() << " bytes to internal SRAM buffer");
        } else {
            // Fallback to direct access (may fail if data is in PSRAM)
            channel->ledSource = ledData.data();
            FL_WARN_ONCE("ChannelEngineSpi: Using direct PSRAM access (ISR-unsafe!) - buffer too small or not allocated");
        }
        channel->ledBytesRemaining = ledData.size();

        // DEBUG: Print first 6 bytes of input data to verify encoding input
        if (ledData.size() >= 6) {
            FL_DBG_EVERY(100, "ChannelEngineSpi: Input LED data (first 6 bytes): ["
                   << static_cast<int>(ledData[0]) << ","
                   << static_cast<int>(ledData[1]) << ","
                   << static_cast<int>(ledData[2]) << ","
                   << static_cast<int>(ledData[3]) << ","
                   << static_cast<int>(ledData[4]) << ","
                   << static_cast<int>(ledData[5]) << "]");

            // DEBUG: Verify encoding works OUTSIDE ISR by testing first byte
            // This helps diagnose if the issue is in the LUT or ISR access
            if (channel->ledSourceBuffer && ledData.size() >= 1) {
                uint8_t test_byte = channel->ledSourceBuffer[0];
                ::fl::Wave8Byte test_output;
                ::fl::detail::wave8_convert_byte_to_wave8byte(test_byte, channel->mWave8Lut, &test_output);
                FL_DBG_EVERY(100, "ChannelEngineSpi: Test encode byte " << static_cast<int>(test_byte)
                       << " → Wave8Byte[0..3]: [" << static_cast<int>(test_output.symbols[0].data)
                       << "," << static_cast<int>(test_output.symbols[1].data)
                       << "," << static_cast<int>(test_output.symbols[2].data)
                       << "," << static_cast<int>(test_output.symbols[3].data) << "]");
            }
        }

        // Store reference to source data for cleanup
        channel->sourceData = data;

        // =========================================================================
        // BLOCKING SPI TRANSMISSION (Cache Coherence Fix - Iteration 5)
        // =========================================================================
        // Use blocking polling transmit from main task. This ensures:
        // 1. SPI driver handles cache coherence internally during polling
        // 2. Transaction completes before returning (no async DMA race conditions)
        // 3. No timer ISR needed for transmission
        //
        // Previous async approach failed due to ESP32-S3 cache coherence issues:
        // - esp_cache_msync() returns ESP_ERR_INVALID_ARG on our buffers
        // - DMA reads stale/zero data from physical memory while CPU cache holds
        //   the correct encoded data
        //
        // Blocking transmit is simpler and works reliably across all ESP32 variants.
        // =========================================================================

        // Encode ALL LED data in main task context
        preEncodeAllData(channel);

        // Transmit using blocking poll (handles cache coherence internally)
        transmitBlockingPolled(channel);

        // Mark transmission as complete (no async waiting needed)
        channel->transmissionComplete = true;
    }
}

ChannelEngineSpi::SpiChannelState *
ChannelEngineSpi::acquireChannel(gpio_num_t pin, const SpiTimingConfig &timing,
                                 size_t dataSize, const ChipsetTimingConfig &originalTiming) {

    // Try to find existing idle channel with matching pin and timing
    for (auto &channel : mChannels) {
        if (!channel.inUse && channel.pin == pin && channel.timing == timing) {
            channel.inUse = true;
            channel.transmissionComplete = false;
            channel.hasNewData = false;
            channel.ledBytesRemaining = 0;

            // CRITICAL FIX: Reinitialize channel if SPI host was released
            // After releaseChannel(), spi_host is set to SPI_HOST_MAX to enable batching
            // We need to reacquire SPI hardware before reusing the channel
            if (channel.spi_host == SPI_HOST_MAX) {
                FL_DBG("ChannelEngineSpi: Reinitializing released channel for pin " << pin);
                if (!createChannel(&channel, pin, timing, dataSize, &originalTiming)) {
                    FL_WARN("ChannelEngineSpi: Failed to reinitialize channel for pin " << pin);
                    channel.inUse = false; // Mark as not in use since reinitialization failed
                    return nullptr;
                }
            }

            return &channel;
        }
    }

    // No matching idle channel - try to create new one
    // CRITICAL: Add to vector FIRST, then initialize, to ensure ISR user_data
    // points to stable memory
    SpiChannelState newChannel = {};
    newChannel.pin = pin;
    newChannel.timing = timing;
    newChannel.inUse = true;
    newChannel.transmissionComplete = false;
    newChannel.hasNewData = false;
    newChannel.spi_host = SPI_HOST_MAX;
    newChannel.spi_device = nullptr;

    // Check if there's a multi-lane configuration for this pin
    auto it = mMultiLaneConfigs.find(pin);
    if (it != mMultiLaneConfigs.end()) {
        // Apply multi-lane configuration
        const MultiLanePinConfig &config = it->second;
        newChannel.numLanes = config.getLaneCount();
        newChannel.data1_pin = config.data1_pin;
        newChannel.data2_pin = config.data2_pin;
        newChannel.data3_pin = config.data3_pin;
        FL_DBG("ChannelEngineSpi: Applying "
               << static_cast<int>(newChannel.numLanes)
               << "-lane configuration for pin " << pin);
    } else {
        // Default to single-lane mode
        newChannel.numLanes = 1;
        newChannel.data1_pin = static_cast<gpio_num_t>(-1);
        newChannel.data2_pin = static_cast<gpio_num_t>(-1);
        newChannel.data3_pin = static_cast<gpio_num_t>(-1);
    }

    // Add to channel list BEFORE calling createChannel() to ensure stable
    // pointer
    mChannels.push_back(newChannel);
    SpiChannelState *channelPtr = &mChannels.back();

    // Now initialize the channel (this will attach ISR with correct pointer)
    // Pass original timing for high-precision wave8 LUT generation
    if (!createChannel(channelPtr, pin, timing, dataSize, &originalTiming)) {
        FL_WARN_ONCE("ChannelEngineSpi: Failed to create channel for pin " << pin);
        // Remove the partially created channel
        mChannels.pop_back();
        return nullptr;
    }

    FL_DBG("ChannelEngineSpi: Created new channel for pin "
           << pin << " (total: " << mChannels.size() << ")");
    return channelPtr;
}

void ChannelEngineSpi::releaseChannel(SpiChannelState *channel) {
    if (channel) {
        // Clear usage flags
        channel->inUse = false;
        channel->transmissionComplete = false;
        channel->hasNewData = false;

        // Reset streaming state (prevents stale data corruption on reuse)
        channel->ledSource = nullptr;
        channel->ledBytesRemaining = 0;
        channel->stagingOffset = 0;
        channel->currentStaging = channel->stagingA; // Reset to buffer A

        // Release reference to source data
        channel->sourceData = nullptr;

        // CRITICAL: Set shutdown flag BEFORE detaching ISR
        // This prevents race condition where ISR fires one more time after detachment
        channel->isShuttingDown = true;

        // Detach timer ISR before releasing hardware resources
        if (channel->timerHandle.is_valid()) {
            fl::isr::detachHandler(channel->timerHandle);
            // Note: detachHandler() invalidates the handle internally
        }

        // Small delay to ensure any in-flight ISR completes (1ms = 1000 ISR ticks at 1kHz)
        // This prevents accessing freed memory if ISR fires between detach and buffer free
        vTaskDelay(pdMS_TO_TICKS(2));

        // CRITICAL FIX: Release SPI host to enable batching
        // Without this, subsequent batches can't acquire SPI hosts
        // The channel remains in mChannels for reuse, but SPI hardware is freed
        // On next use, acquireChannel() will find this idle channel and reinitialize it
        if (channel->spi_device) {
            // Drain all pending transactions BEFORE removing device
            // This prevents "Have unfinished transactions" errors from ESP-IDF
            // Note: spi_device_get_trans_result() returns ESP_ERR_TIMEOUT if no transactions are queued
            // We loop until we get a timeout, indicating the queue is empty
            spi_transaction_t *trans_ptr;
            esp_err_t result;
            int drained = 0;
            const int max_drain = 10; // Safety limit to prevent infinite loop

            while (drained < max_drain) {
                result = spi_device_get_trans_result(channel->spi_device, &trans_ptr, pdMS_TO_TICKS(100));
                if (result == ESP_ERR_TIMEOUT) {
                    // No more transactions in queue
                    break;
                } else if (result == ESP_OK) {
                    // Transaction completed
                    drained++;
                } else {
                    // Other error - log and break
                    FL_WARN_ONCE("ChannelEngineSpi: spi_device_get_trans_result failed during drain: " << result);
                    break;
                }
            }

            if (drained > 0) {
                FL_DBG_EVERY(100, "ChannelEngineSpi: Drained " << drained << " pending transactions before device removal");

                // DEBUG: Dump first 16 bytes of staging buffer A to see what was actually transmitted
                // This helps diagnose if the ISR is encoding correctly
                if (channel->stagingA) {
                    FL_DBG_EVERY(100, "ChannelEngineSpi: Staging buffer A (first 16 bytes): ["
                           << static_cast<int>(channel->stagingA[0]) << ","
                           << static_cast<int>(channel->stagingA[1]) << ","
                           << static_cast<int>(channel->stagingA[2]) << ","
                           << static_cast<int>(channel->stagingA[3]) << ","
                           << static_cast<int>(channel->stagingA[4]) << ","
                           << static_cast<int>(channel->stagingA[5]) << ","
                           << static_cast<int>(channel->stagingA[6]) << ","
                           << static_cast<int>(channel->stagingA[7]) << " | "
                           << static_cast<int>(channel->stagingA[8]) << ","
                           << static_cast<int>(channel->stagingA[9]) << ","
                           << static_cast<int>(channel->stagingA[10]) << ","
                           << static_cast<int>(channel->stagingA[11]) << ","
                           << static_cast<int>(channel->stagingA[12]) << ","
                           << static_cast<int>(channel->stagingA[13]) << ","
                           << static_cast<int>(channel->stagingA[14]) << ","
                           << static_cast<int>(channel->stagingA[15]) << "]");

                    // DEBUG: Also check last transaction size (from transA/B structures)
                    FL_DBG_EVERY(100, "ChannelEngineSpi: TransA length=" << (channel->transA.length / 8) << " bytes"
                           << ", TransB length=" << (channel->transB.length / 8) << " bytes");

                    // DEBUG: Verify tx_buffer addresses match staging buffers
                    FL_DBG_EVERY(100, "ChannelEngineSpi: Buffer addresses - stagingA=" << fl::ptr_to_int(channel->stagingA)
                           << ", stagingB=" << fl::ptr_to_int(channel->stagingB)
                           << ", transA.tx_buffer=" << fl::ptr_to_int(channel->transA.tx_buffer)
                           << ", transB.tx_buffer=" << fl::ptr_to_int(channel->transB.tx_buffer));

                    // DEBUG: Print captured tx_buffer from ISR
                    if (channel->debugTxCaptured) {
                        FL_DBG_EVERY(100, "ChannelEngineSpi: ISR tx_buffer (captured at queue): ["
                               << static_cast<int>(channel->debugTxBuffer[0]) << ","
                               << static_cast<int>(channel->debugTxBuffer[1]) << ","
                               << static_cast<int>(channel->debugTxBuffer[2]) << ","
                               << static_cast<int>(channel->debugTxBuffer[3]) << ","
                               << static_cast<int>(channel->debugTxBuffer[4]) << ","
                               << static_cast<int>(channel->debugTxBuffer[5]) << ","
                               << static_cast<int>(channel->debugTxBuffer[6]) << ","
                               << static_cast<int>(channel->debugTxBuffer[7]) << "]");
                    } else {
                        FL_WARN_EVERY(100, "ChannelEngineSpi: ISR tx_buffer NOT captured (debugTxCaptured=false)");
                    }
                }
            }

            // Clear transaction flags after draining
            channel->transAInFlight = false;
            channel->transBInFlight = false;

            // Remove SPI device from bus
            spi_bus_remove_device(channel->spi_device);
            channel->spi_device = nullptr;
        } else {
            // No device, but clear flags anyway
            channel->transAInFlight = false;
            channel->transBInFlight = false;
        }

        if (channel->spi_host != SPI_HOST_MAX) {
            // Free SPI bus and decrement host refCount
            releaseSpiHost(channel->spi_host);
            channel->spi_host = SPI_HOST_MAX;
        }

        // Free staging buffers to prevent memory leak on reinitialization
        // (allocated with heap_caps_aligned_alloc, freed with heap_caps_free())
        if (channel->stagingA) {
            heap_caps_free(channel->stagingA);  // heap_caps_aligned_alloc uses heap_caps_free()
            channel->stagingA = nullptr;
        }
        if (channel->stagingB) {
            heap_caps_free(channel->stagingB);
            channel->stagingB = nullptr;
        }
        channel->stagingCapacity = 0;
    }
}

bool ChannelEngineSpi::createChannel(SpiChannelState *state, gpio_num_t pin,
                                     const SpiTimingConfig &timing,
                                     size_t dataSize, const ChipsetTimingConfig *originalTiming) {
    // Safety counter to detect infinite channel creation loops
    static int creation_count = 0;
    static uint32_t last_reset_ms = 0;
    uint32_t now_ms = millis();

    // Reset counter every 5 seconds
    if (now_ms - last_reset_ms > 5000) {
        creation_count = 0;
        last_reset_ms = now_ms;
    }

    creation_count++;
    FL_DBG_EVERY(10, "ChannelEngineSpi: Creating channel for pin " << pin
           << " (attempt " << creation_count << " in last 5s)");

    if (creation_count > 100) {
        FL_ERROR("ChannelEngineSpi: ABORT - Too many channel creation attempts ("
                 << creation_count << " in 5s). Possible infinite loop or resource leak.");
        return false;
    }

    // Acquire SPI host
    state->spi_host = acquireSpiHost();
    if (state->spi_host == SPI_HOST_MAX) {
        FL_WARN_EVERY(10, "ChannelEngineSpi: No available SPI host (attempt " << creation_count << ")");
        return false;
    }

    // Calculate buffer size for wave8 encoding (8 SPI bits per LED bit)
    // Total bits = dataSize * 8 LED bits/byte * 8 SPI bits/LED bit = dataSize * 64
    // Convert to bytes = dataSize * 8
    const size_t spiBufferSize = dataSize * 8;

    // Determine if we should use DMA
    // Use DMA for larger buffers (>64 bytes) similar to Espressif driver
    state->useDMA = (spiBufferSize > 64);

    // =========================================================================
    // =========================================================================
    // ITERATION 14 INVESTIGATION: gpio_reset_pin() behavior
    // =========================================================================
    // Previously we called gpio_reset_pin() to disconnect from UART, but
    // Espressif's led_strip driver does NOT call gpio_reset_pin() at all.
    // They rely solely on spi_bus_initialize() to configure the GPIO.
    //
    // However, we still call it because:
    // 1. GPIO 1 may be connected to UART0 TX on startup
    // 2. Without reset, the UART signal could interfere with SPI
    //
    // If this continues to fail, try commenting out gpio_reset_pin() to test.
    // =========================================================================
    gpio_reset_pin(pin);
    FL_DBG("ChannelEngineSpi: Reset GPIO " << static_cast<int>(pin) << " to disconnect from previous peripheral");

    // Configure SPI bus with multi-lane support
    spi_bus_config_t bus_config = {};
    bus_config.mosi_io_num = pin; // Data0 (always present)

    // =========================================================================
    // ESP32-S3 FSPI DMA FIX: Set MISO to a valid GPIO even when unused
    // =========================================================================
    // Known issue on ESP32-S3: When using SPI2 (FSPI) with DMA and MISO=-1,
    // MOSI output doesn't work (stays low). The workaround is to set MISO
    // to a valid unused GPIO. This is applied after SCLK is determined below.
    //
    // Reference: https://github.com/Bodmer/TFT_eSPI/discussions/2233
    // "FSPI still doesn't work with DMA... MOSI stays at low level"
    // =========================================================================
    bus_config.miso_io_num =
        state->data1_pin;        // Data1 for dual/quad mode (-1 if unused)

    // ⚠️ CLOCKLESS-OVER-SPI ARCHITECTURE: Clock pin is internal-only, NOT connected to LEDs!
    //
    // CRITICAL: The SPI peripheral requires a clock signal for precise MOSI timing generation,
    // BUT this clock is NEVER physically connected to the LED strip. This is what makes this
    // a "clockless-over-SPI" driver:
    //
    //   1. ESP32 SPI peripheral generates clock internally (e.g., 6.5MHz for wave8 encoding)
    //   2. This clock controls the exact timing of MOSI bit transitions
    //   3. MOSI pin sends encoded bit patterns to the LEDs (wave8: 8 SPI bits per LED bit)
    //   4. Clock pin is NOT used - set to -1 like Espressif led_strip driver
    //   5. LEDs decode the data stream based on pulse widths (T0H/T0L vs T1H/T1L)
    //
    // =========================================================================
    // ITERATION 6 FIX: Match Espressif led_strip driver configuration EXACTLY
    // =========================================================================
    // Espressif's led_strip_spi_dev.c uses:
    //   - sclk_io_num = -1 (no clock pin)
    //   - miso_io_num = -1 (no MISO pin)
    //   - Only mosi_io_num is set
    //
    // Previous iterations tried real SCLK/MISO GPIOs but this didn't help.
    // Now matching Espressif's exact configuration for consistency.
    // =========================================================================
    bus_config.sclk_io_num = -1;  // Match Espressif: no clock pin
    bus_config.miso_io_num = -1;  // Match Espressif: no MISO pin

    FL_DBG("ChannelEngineSpi: SPI bus config - MOSI=" << static_cast<int>(pin)
           << ", SCLK=-1 (internal), MISO=-1 (unused)"
           << ", host=" << static_cast<int>(state->spi_host));

    // Warn if using non-IO_MUX pin for MOSI (may have GPIO matrix routing issues)
#if defined(FL_IS_ESP_32S3)
    // ESP32-S3: GPIO 11 is native SPI2 MOSI, GPIO 37 is native SPI3 MOSI
    // Other pins go through GPIO matrix which works but may have timing variations
    if (state->spi_host == SPI3_HOST && pin != GPIO_NUM_37) {
        FL_DBG("ChannelEngineSpi: GPIO " << static_cast<int>(pin) << " routed via GPIO matrix (not native SPI3 MOSI)");
    }
#endif
    bus_config.quadwp_io_num =
        state->data2_pin; // Data2 for quad mode (-1 if unused)
    bus_config.quadhd_io_num =
        state->data3_pin; // Data3 for quad mode (-1 if unused)
    bus_config.max_transfer_sz = spiBufferSize;

    // Set flags based on lane count (1, 2, or 4 lanes)
    bus_config.flags = SPICOMMON_BUSFLAG_MASTER;
    if (state->numLanes >= 4) {
        bus_config.flags |= SPICOMMON_BUSFLAG_QUAD; // Quad mode (4 lanes)
    } else if (state->numLanes >= 2) {
        bus_config.flags |= SPICOMMON_BUSFLAG_DUAL; // Dual mode (2 lanes)
    }
    // else: standard SPI (1 data line)

    // Initialize SPI bus
    // =========================================================================
    // ITERATION 7 FINDINGS: Non-DMA mode also fails
    // =========================================================================
    // Testing revealed that the ~7µs high pulse at start occurs even when the
    // SPI transaction is rejected (transfer too large for non-DMA mode). This
    // indicates the pulse is NOT from actual data transmission - it's likely
    // from GPIO initialization or bus acquisition.
    //
    // The real issue: GPIO matrix is not routing SPI MOSI signal to the pin.
    // The SPI peripheral transmits correctly (proven by timing), but the
    // MOSI output isn't appearing on the physical GPIO.
    //
    // Using DMA for now since non-DMA has 64-byte limit.
    // =========================================================================
    esp_err_t ret =
        spi_bus_initialize(state->spi_host, &bus_config, SPI_DMA_CH_AUTO);
    if (ret != ESP_OK) {
        FL_WARN("ChannelEngineSpi: spi_bus_initialize failed: " << ret);
        releaseSpiHost(state->spi_host);
        state->spi_host = SPI_HOST_MAX;
        return false;
    }

    // Configure SPI device
    // =========================================================================
    // ITERATION 13 FIX: Use calculated wave8 clock, NOT hardcoded 2.5MHz
    // =========================================================================
    // The wave8 LUT is built based on the chipset timing period. Each LED bit
    // expands to 8 SPI bits. To maintain correct timing, the SPI clock must be:
    //   wave8_clock_hz = 8 / period_ns * 1e9
    //
    // For WS2812B: period=1225ns → wave8_clock = 6.53MHz → ~153ns per SPI bit
    //   - bit0: 1 high pulse = 153ns (T0H target=225ns, acceptable)
    //   - bit1: 4 high pulses = 612ns (T1H target=580ns, good match)
    //
    // CRITICAL: Using 2.5MHz was WRONG because:
    //   - At 2.5MHz: 1 pulse = 400ns, 4 pulses = 1600ns (2.6x too slow!)
    //   - The LUT was built for 6.5MHz timing, not 2.5MHz
    //   - This caused the RX to see only 1 long pulse instead of proper waveforms
    // =========================================================================
    uint32_t wave8_clock_hz;
    if (originalTiming) {
        const uint32_t period_ns = originalTiming->t1_ns + originalTiming->t2_ns + originalTiming->t3_ns;
        wave8_clock_hz = (8ULL * 1000000000ULL) / period_ns;
        FL_DBG("ChannelEngineSpi: Calculated wave8 clock: " << wave8_clock_hz << "Hz (period=" << period_ns << "ns)");
    } else {
        // Fallback: use SPI timing's clock (already wave8-adjusted)
        wave8_clock_hz = timing.clock_hz;
        FL_DBG("ChannelEngineSpi: Using SPI timing clock: " << wave8_clock_hz << "Hz");
    }

    spi_device_interface_config_t dev_config = {};
    dev_config.command_bits = 0;
    dev_config.address_bits = 0;
    dev_config.dummy_bits = 0;
    dev_config.clock_speed_hz = wave8_clock_hz;
    dev_config.mode = 0;          // SPI mode 0
    dev_config.spics_io_num = -1; // No CS pin
    dev_config.queue_size = 4;    // Transaction queue size
    dev_config.post_cb =
        spiPostTransactionCallback; // ISR callback when transaction completes

    FL_DBG("ChannelEngineSpi: SPI clock_hz=" << wave8_clock_hz
           << " (wave8 adjusted from " << timing.clock_hz << ")"
           << ", bits_per_led_bit=8 (wave8)"
           << ", buffer_size=" << spiBufferSize << " bytes");

    // =========================================================================
    // ITERATION 7: Try SPI_DEVICE_HALFDUPLEX for TX-only operation
    // =========================================================================
    // For LED strip driving, we only transmit (TX) and never receive (RX).
    // Half-duplex mode is designed for such unidirectional operations.
    //
    // Previous iterations used full-duplex mode (flags=0) which may have
    // caused issues with GPIO output configuration on ESP32-S3.
    //
    // Reference: ESP-IDF documentation recommends HALFDUPLEX for TX-only
    // or RX-only operations when DMA is involved.
    // =========================================================================
    dev_config.flags = SPI_DEVICE_HALFDUPLEX;

    // Add device to bus
    ret = spi_bus_add_device(state->spi_host, &dev_config, &state->spi_device);
    if (ret != ESP_OK) {
        FL_WARN("ChannelEngineSpi: spi_bus_add_device failed: " << ret);
        spi_bus_free(state->spi_host);
        releaseSpiHost(state->spi_host);
        state->spi_host = SPI_HOST_MAX;
        return false;
    }

    // Verify actual clock frequency
    int actual_freq_khz = 0;
    spi_device_get_actual_freq(state->spi_device, &actual_freq_khz);
    int requested_freq_khz = timing.clock_hz / 1000;
    FL_DBG("ChannelEngineSpi: Actual SPI clock frequency: " << actual_freq_khz << " kHz");
    if (actual_freq_khz < requested_freq_khz - 300 ||
        actual_freq_khz > requested_freq_khz + 300) {
        FL_WARN_ONCE("ChannelEngineSpi: Clock frequency mismatch - requested "
                << requested_freq_khz << " kHz, actual " << actual_freq_khz
                << " kHz");
    }

    // DEBUG: Verify GPIO is configured as output by using ESP-IDF API
    // After spi_bus_initialize, the MOSI pin should be set as output
    int gpio_level = gpio_get_level(pin);
    FL_DBG("ChannelEngineSpi: GPIO " << static_cast<int>(pin) << " current level: " << gpio_level);

    // =========================================================================
    // ITERATION 6 FIX: Manual GPIO Matrix MOSI Signal Routing
    // =========================================================================
    // The ESP-IDF SPI driver should configure the GPIO matrix automatically,
    // but on ESP32-S3, the MOSI signal may not be properly routed to the GPIO pin.
    //
    // DIAGNOSIS (Iteration 5):
    // - SPI peripheral is working: 8ms transmit time for 2400 bytes @ 2.5MHz ✓
    // - GPIO baseline test passes: manual digitalWrite works on GPIO 1 ✓
    // - BUT: MOSI signal not appearing on GPIO - RMT RX captures 0 edges ✗
    //
    // ROOT CAUSE HYPOTHESIS: GPIO matrix not routing SPI MOSI output to GPIO pin.
    //
    // FIX: Manually connect the SPI MOSI output signal to the GPIO pin via GPIO matrix.
    // This is the same technique used by Espressif's led_strip driver for output inversion.
    //
    // Signal IDs (from soc/gpio_sig_map.h):
    // - SPI2 (FSPI): FSPID_OUT_IDX (signal 66 on ESP32-S3)
    // - SPI3 (HSPI): SPI3_D_OUT_IDX (signal 92 on ESP32-S3)
    //
    // Reference: Espressif led_strip_spi_dev.c uses esp_rom_gpio_connect_out_signal()
    // =========================================================================
    // =========================================================================
    // ITERATION 8 FIX: RE-ADD manual GPIO matrix routing
    // =========================================================================
    // Research from GitHub issues (#13478, #12759) and ESP32 forum shows that
    // the ESP-IDF SPI driver may NOT properly configure GPIO matrix routing
    // for non-IO_MUX pins on ESP32-S3.
    //
    // Evidence:
    // - GitHub #13478: "MISO line seems to not register, it is reading only LOW"
    //   when using custom SPI pins via GPIO matrix on ESP32-S3
    // - GitHub #12759: SPI MOSI output fixed by calling esp_rom_gpio_connect_out_signal()
    //   AFTER spi_bus_initialize()
    //
    // The workaround is to explicitly call esp_rom_gpio_connect_out_signal()
    // after SPI bus initialization to ensure proper GPIO matrix routing.
    //
    // Signal IDs (from soc/spi_periph.h via spi_periph_signal[host].spid_out):
    // - SPI2_HOST (FSPI): spi_periph_signal[SPI2_HOST].spid_out
    // - SPI3_HOST (HSPI): spi_periph_signal[SPI3_HOST].spid_out
    //
    // Parameters for esp_rom_gpio_connect_out_signal():
    // - gpio_num: The GPIO pin to route to
    // - signal_idx: The peripheral signal ID (MOSI output)
    // - out_inv: false (no output inversion - WS2812 needs non-inverted data)
    // - out_en_inv: false (no output enable inversion)
    // =========================================================================

    // =========================================================================
    // ITERATION 14: RE-ADD MANUAL GPIO ROUTING (CORRECTED)
    // =========================================================================
    // The ESP-IDF SPI driver SHOULD handle GPIO matrix routing automatically,
    // but ESP32-S3 with non-IO_MUX pins (like GPIO 1) may need manual help.
    //
    // Espressif's led_strip driver calls esp_rom_gpio_connect_out_signal()
    // only for output inversion, but some GitHub issues suggest it's needed
    // for proper GPIO matrix connection on non-IO_MUX pins.
    //
    // Signal IDs from soc/spi_periph.h via spi_periph_signal[host].spid_out:
    // - SPI2_HOST (FSPI): 103 on ESP32-S3
    // - SPI3_HOST (HSPI): varies by chip
    //
    // Parameters for esp_rom_gpio_connect_out_signal():
    // - gpio_num: The GPIO pin to route to
    // - signal_idx: The peripheral signal ID (MOSI output)
    // - out_inv: false (no output inversion - WS2812 needs non-inverted data)
    // - out_en_inv: false (no output enable inversion)
    // =========================================================================

    // Get the correct MOSI signal for the SPI host
    int mosi_signal = spi_periph_signal[state->spi_host].spid_out;

    // Manually route SPI MOSI output to the GPIO pin via GPIO matrix
    esp_rom_gpio_connect_out_signal(pin, mosi_signal, false, false);

    // Ensure GPIO is configured as output
    gpio_set_direction(pin, GPIO_MODE_OUTPUT);

    FL_DBG("ChannelEngineSpi: Manually routed MOSI signal " << mosi_signal
           << " to GPIO " << static_cast<int>(pin) << " via GPIO matrix");

    // NOTE: Removed test transmission during channel creation (Iteration 8)
    // The test transmission interfered with validation RX capture by triggering
    // edge detection before the actual LED data was sent. The test pattern
    // (0xFF 0x00 alternating) produced ~30µs of edges that the RMT RX captured
    // instead of the actual LED data.
    //
    // If SPI output verification is needed, it should be done outside the
    // show() path when RX is not armed.

    // Allocate double-buffered staging buffers (4KB each)
    //
    // =========================================================================
    // ESP32-S3 DMA BUFFER ALLOCATION (Iteration 12):
    // =========================================================================
    // Following Espressif's led_strip driver pattern:
    // - MALLOC_CAP_INTERNAL: Forces allocation in internal SRAM
    // - MALLOC_CAP_DMA: Ensures DMA-accessible memory alignment
    //
    // Using internal DMA memory eliminates cache coherence issues. The internal
    // SRAM on ESP32-S3 is directly accessible by DMA without cache intervention,
    // so no esp_cache_msync() calls are needed.
    //
    // Reference: idf-extra-components/led_strip/src/led_strip_spi_dev.c
    // =========================================================================
    const size_t staging_size = 4096;

    state->stagingA = (uint8_t *)heap_caps_malloc(
        staging_size, MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA);
    state->stagingB = (uint8_t *)heap_caps_malloc(
        staging_size, MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA);

    if (!state->stagingA || !state->stagingB) {
        FL_WARN("ChannelEngineSpi: Failed to allocate staging buffers");
        if (state->stagingA)
            heap_caps_free(state->stagingA);
        if (state->stagingB)
            heap_caps_free(state->stagingB);
        spi_bus_remove_device(state->spi_device);
        spi_bus_free(state->spi_host);
        releaseSpiHost(state->spi_host);
        return false;
    }

    // Zero the staging buffers - wave8 encoding assumes zero-initialized memory
    fl::memset(state->stagingA, 0, staging_size);
    fl::memset(state->stagingB, 0, staging_size);

    FL_DBG("ChannelEngineSpi: Allocated staging buffers via heap_caps_malloc (INTERNAL|DMA)");

    state->stagingCapacity = staging_size;
    state->currentStaging = state->stagingA;
    state->stagingOffset = 0;

    // Allocate LED source buffer (internal SRAM for ISR-safe access)
    // CRITICAL: PSRAM is NOT safe for ISR access - we must copy LED data here
    // Allocate for typical LED strip size (dataSize is passed from channel data)
    state->ledSourceBuffer = (uint8_t *)heap_caps_malloc(
        dataSize, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    state->ledSourceBufferSize = state->ledSourceBuffer ? dataSize : 0;

    if (!state->ledSourceBuffer) {
        FL_WARN("ChannelEngineSpi: Failed to allocate LED source buffer ("
                << dataSize << " bytes) - falling back to direct access");
        // Continue without buffer - will try direct access (may fail on PSRAM)
    }

    // Initialize streaming state
    state->hasNewData = false;
    state->isShuttingDown = false;  // Clear shutdown flag for new/reused channel
    state->cacheSyncDisabled = false;  // Enable cache sync for new channel (will auto-disable if fails)
    state->ledSource = nullptr;
    state->ledBytesRemaining = 0;
    state->transAInFlight = false;
    state->transBInFlight = false;

    // CRITICAL: Zero-initialize transaction structures to avoid stale data
    // spi_transaction_t has many fields that must be properly initialized
    fl::memset(&state->transA, 0, sizeof(spi_transaction_t));
    fl::memset(&state->transB, 0, sizeof(spi_transaction_t));

    // Build wave8 expansion LUT once during channel creation (not in ISR)
    // CRITICAL FIX: Use original chipset timing for wave8 LUT if available
    // This provides higher precision than the SPI-quantized timing.
    // The SPI timing is quantized to bit patterns (e.g., 3-bit for WS2812),
    // which loses timing precision. The original ChipsetTimingConfig has
    // nanosecond-precise values that produce a more accurate wave8 LUT.
    ChipsetTiming chipsetTiming;
    if (originalTiming) {
        // Use original timing for maximum precision
        chipsetTiming.T1 = originalTiming->t1_ns;
        chipsetTiming.T2 = originalTiming->t2_ns;
        chipsetTiming.T3 = originalTiming->t3_ns;
        chipsetTiming.RESET = originalTiming->reset_us;
        chipsetTiming.name = originalTiming->name;
        FL_DBG("ChannelEngineSpi: Using ORIGINAL chipset timing for wave8 LUT (high precision)");
    } else {
        // Fall back to SPI-derived timing (lower precision due to quantization)
        chipsetTiming = convertSpiTimingToChipsetTiming(timing);
        FL_DBG("ChannelEngineSpi: Using SPI-derived timing for wave8 LUT (reduced precision)");
    }
    state->mWave8Lut = buildWave8ExpansionLUT(chipsetTiming);

    // CRITICAL: Memory barriers to ensure LUT is written before ISR can access it
    // This fixes the race condition where early ISR fires see zero-filled LUT entries
    asm volatile("" ::: "memory");  // Compiler barrier prevents instruction reordering
    __sync_synchronize();           // Hardware memory barrier ensures cache flush

    // Verify LUT is non-zero (diagnostic for race condition bug)
    // LUT structure: lut[16][4] - 16 nibbles, 4 Wave8Bit bytes per nibble
    bool lut_valid = false;
    for (size_t nibble = 0; nibble < 16 && !lut_valid; ++nibble) {
        for (size_t byte = 0; byte < 4 && !lut_valid; ++byte) {
            if (state->mWave8Lut.lut[nibble][byte].data != 0) {
                lut_valid = true;
            }
        }
    }
    if (!lut_valid) {
        FL_WARN("ChannelEngineSpi: Wave8 LUT appears to be all zeros - memory barrier may have failed!");
    }

    // Calculate wave8 clock for debug output (8 bits per LED bit instead of timing.bits_per_led_bit)
    const uint32_t wave8_clk = (8 * timing.clock_hz) / timing.bits_per_led_bit;
    FL_DBG("ChannelEngineSpi: Initialized wave8 LUT for channel (lut_valid=" << lut_valid << ", usedOriginalTiming=" << (originalTiming ? 1 : 0) << ")");
    FL_DBG("  SPI Clock: " << wave8_clk << " Hz (wave8, 8 bits per LED bit)");
    FL_DBG("  Chipset Timing: T0H=" << chipsetTiming.T1 << "ns, T1H=" << (chipsetTiming.T1 + chipsetTiming.T2)
           << "ns, T0L=" << chipsetTiming.T3 << "ns");
    // Log LUT entries to verify bit0 vs bit1 waveforms
    // nibble[0] = 0b0000 → all bit0_waveforms
    // nibble[15] = 0b1111 → all bit1_waveforms
    FL_DBG("  LUT nibble[0] (all 0s): ["
           << static_cast<int>(state->mWave8Lut.lut[0][0].data) << ","
           << static_cast<int>(state->mWave8Lut.lut[0][1].data) << ","
           << static_cast<int>(state->mWave8Lut.lut[0][2].data) << ","
           << static_cast<int>(state->mWave8Lut.lut[0][3].data) << "]");
    FL_DBG("  LUT nibble[15] (all 1s): ["
           << static_cast<int>(state->mWave8Lut.lut[15][0].data) << ","
           << static_cast<int>(state->mWave8Lut.lut[15][1].data) << ","
           << static_cast<int>(state->mWave8Lut.lut[15][2].data) << ","
           << static_cast<int>(state->mWave8Lut.lut[15][3].data) << "]");

    // Set up timer ISR (1 kHz = 1ms period, reduced from 4kHz to avoid stack overflow)
    // The ISR encodes LED data into SPI staging buffers. Lower frequency reduces
    // CPU overhead at the cost of slightly longer initial latency.

    // Check stack - WARN if critically low (< 700 bytes)
    // Note: attachTimerHandler needs ~500-600 bytes for ESP-IDF gptimer setup
    {
        UBaseType_t hwm = uxTaskGetStackHighWaterMark(nullptr);
        if (hwm < 700) {
            FL_WARN_ONCE("ChannelEngineSpi: Low stack: " << hwm << "B");
        }
    }

    // Set up timer ISR at 1kHz for encoding chunks
    fl::isr::isr_config_t isr_config;
    isr_config.handler = timerEncodingISR;
    isr_config.user_data = state;
    isr_config.frequency_hz = 1000;
    isr_config.flags = 0;

    fl::isr::isr_handle_t isr_handle;
    int isr_ret = fl::isr::attachTimerHandler(isr_config, &isr_handle);
    if (isr_ret != 0) {
        FL_WARN_ONCE("ChannelEngineSpi: Timer attach failed: " << isr_ret);
        heap_caps_free(state->stagingA);  // heap_caps_aligned_alloc uses heap_caps_free()
        heap_caps_free(state->stagingB);
        spi_bus_remove_device(state->spi_device);
        spi_bus_free(state->spi_host);
        releaseSpiHost(state->spi_host);
        return false;
    }

    state->timerHandle = isr_handle;

    // Initialize debug capture state
    state->debugTxCaptured = false;
    for (int i = 0; i < 8; i++) {
        state->debugTxBuffer[i] = 0;
    }

    FL_DBG_EVERY(10, "ChannelEngineSpi: Created pin=" << pin
           << " lanes=" << static_cast<int>(state->numLanes)
           << " host=" << state->spi_host);

    return true;
}

bool ChannelEngineSpi::encodeLedData(const fl::span<const uint8_t> &ledData,
                                     fl::vector<uint8_t> &spiBuffer,
                                     const SpiTimingConfig &timing) {
    // Calculate required SPI buffer size (variable expansion ratio)
    // Total bits = ledData.size() * 8 * timing.bits_per_led_bit
    // Convert to bytes (round up)
    const size_t total_bits = ledData.size() * 8 * timing.bits_per_led_bit;
    const size_t spiSize = (total_bits + 7) / 8; // Round up to nearest byte

    spiBuffer.clear();
    spiBuffer.resize(spiSize);

    // Zero the buffer (required for bit encoding)
    fl::memset(spiBuffer.data(), 0, spiSize);

    // Encode each LED byte using dynamic pattern
    uint32_t output_bit_offset = 0;
    for (size_t i = 0; i < ledData.size(); i++) {
        output_bit_offset += encodeLedByte(ledData[i], spiBuffer.data(), timing,
                                           output_bit_offset);
    }

    return true;
}

uint32_t ChannelEngineSpi::encodeLedByte(uint8_t data, uint8_t *buf,
                                         const SpiTimingConfig &timing,
                                         uint32_t output_bit_offset) {
    // Dynamic encoding: Each LED bit expands to timing.bits_per_led_bit SPI
    // bits using the bit patterns from timing.bit0_pattern and
    // timing.bit1_pattern
    //
    // Process LED byte MSB first (bit 7 → bit 0)

    uint32_t current_bit_offset = output_bit_offset;
    uint32_t last_byte_index = 0xFFFFFFFF; // Track which byte we're writing to

    for (int led_bit = 7; led_bit >= 0; led_bit--) {
        // Select pattern based on LED bit value
        const uint32_t pattern =
            (data & (1 << led_bit)) ? timing.bit1_pattern : timing.bit0_pattern;
        const uint8_t pattern_bits = timing.bits_per_led_bit;

        // Write pattern bits to buffer (MSB first)
        for (uint8_t i = 0; i < pattern_bits; i++) {
            const uint32_t byte_index = current_bit_offset / 8;
            const uint8_t bit_index = 7 - (current_bit_offset % 8); // MSB first

            // Zero the byte when we first touch it
            if (byte_index != last_byte_index) {
                buf[byte_index] = 0;
                last_byte_index = byte_index;
            }

            // Get bit from pattern (MSB first)
            const uint8_t pattern_bit = (pattern >> (pattern_bits - 1 - i)) & 1;

            if (pattern_bit) {
                // Set bit in output buffer
                buf[byte_index] |= (1 << bit_index);
            }

            current_bit_offset++;
        }
    }

    return current_bit_offset -
           output_bit_offset; // Return number of bits written
}

spi_host_device_t ChannelEngineSpi::acquireSpiHost() {
    // POLICY: Use SPI3_HOST on ESP32-S3 to avoid FSPI+DMA issues
    //
    // ESP32-S3 FSPI (SPI2_HOST) has known DMA issues when MISO=-1:
    // - DMA RX/TX enable bits in SPI_DMA_CONF_REG affect operation even for TX-only
    // - Reference: https://github.com/Bodmer/TFT_eSPI/discussions/2233
    // - Workaround: Use HSPI (SPI3_HOST) instead of FSPI (SPI2_HOST)
    //
    // For other ESP32 variants, SPI2_HOST works fine.
    //
    // Multi-lane SPI (dual/quad) is still supported on the SINGLE host.
    // Sequential channel transmission through batching handles multiple strips.
    static const spi_host_device_t hosts[] = {
// ITERATION 5: Try SPI2_HOST on ESP32-S3
// Previous iterations showed SPI3 transmission taking 8ms (correct!) but GPIO
// output not appearing. SPI2 has its own dedicated DMA channel and may have
// different GPIO matrix routing behavior.
#if defined(SPI2_HOST)
        SPI2_HOST,  // Try FSPI - SPI2 has its own DMA channel
#endif
#if defined(FL_IS_ESP_32S3) && defined(SPI3_HOST)
        SPI3_HOST,  // Fallback to HSPI if SPI2 fails
#endif
        // SPI1_HOST intentionally omitted - reserved for flash
    };
    static const size_t num_hosts = sizeof(hosts) / sizeof(hosts[0]);

    // Try to find an available host
    for (size_t i = 0; i < num_hosts; i++) {
        spi_host_device_t host = hosts[i];

        // Find or create tracking entry
        SpiHostTracking *tracking = nullptr;
        for (auto &entry : sSpiHostUsage) {
            if (entry.host == host) {
                tracking = &entry;
                break;
            }
        }

        if (!tracking) {
            // First use of this host - create tracking entry
            SpiHostTracking newTracking = {};
            newTracking.host = host;
            newTracking.refCount = 0;
            newTracking.initialized = false;
            newTracking.activeLanes = 0; // No lanes in use yet
            sSpiHostUsage.push_back(newTracking);
            tracking = &sSpiHostUsage.back();
        }

        // ESP32 SPI limitation: Each host can only have one bus configuration
        // This means one set of pins (data0/data1/data2/data3) per host
        // Multi-lane (dual/quad) uses multiple pins on the SAME host
        // So we limit to refCount == 0 (not yet in use)
        //
        // Future enhancement: Could allow multiple channels to share the same
        // host if they have identical pin configurations (bus sharing)
        if (tracking->refCount == 0) {
            tracking->refCount++;
            tracking->initialized = true;
            FL_DBG("ChannelEngineSpi: Acquired SPI host "
                   << host << " (refCount=" << tracking->refCount << ")");
            return host;
        }
    }

    // Single SPI host policy: Only one SPI host is used
    // If we get here, the preferred host is already in use - this is expected behavior
    // for multiple channels. The caller will queue the channel for later.
    FL_DBG_EVERY(10, "ChannelEngineSpi: SPI host in use, channel will be queued for sequential transmission");
    return SPI_HOST_MAX;
}

void ChannelEngineSpi::releaseSpiHost(spi_host_device_t host) {
    for (auto &entry : sSpiHostUsage) {
        if (entry.host == host) {
            if (entry.refCount > 0) {
                entry.refCount--;
                FL_DBG("ChannelEngineSpi: Released SPI host "
                       << host << " (refCount=" << entry.refCount << ")");

                if (entry.refCount == 0) {
                    // Free the SPI bus
                    spi_bus_free(host);
                    entry.initialized = false;
                    FL_DBG("ChannelEngineSpi: Freed SPI bus " << host);
                }
            }
            return;
        }
    }
}

SpiTimingConfig
ChannelEngineSpi::calculateSpiTiming(const ChipsetTimingConfig &chipsetTiming) {
    // Use symmetric 3-phase timing (T1/T2/T3) directly
    // All LED timings are symmetric: both bit 0 and bit 1 have period = T1 + T2
    // + T3
    //
    // For bit '0': high for T1, low for T2+T3
    // For bit '1': high for T1+T2, low for T3

    const uint32_t t1_ns = chipsetTiming.t1_ns;
    const uint32_t t2_ns = chipsetTiming.t2_ns;
    const uint32_t t3_ns = chipsetTiming.t3_ns;

    // Find GCD of original timings to get the optimal time quantum
    // This provides best memory efficiency while maintaining perfect timing
    // accuracy
    uint32_t quantum_ns = gcd(gcd(t1_ns, t2_ns), t3_ns);

    // Ensure minimum quantum of 10ns to avoid excessively high frequencies
    if (quantum_ns < 10) {
        FL_WARN_ONCE("ChannelEngineSpi: Quantum too small ("
                << quantum_ns << "ns), rounding up to 10ns");
        quantum_ns = 10;
    }

    // Calculate how many quanta each phase needs (use original timings)
    const uint32_t t1_quanta =
        (t1_ns + quantum_ns / 2) / quantum_ns; // Round to nearest
    const uint32_t t2_quanta = (t2_ns + quantum_ns / 2) / quantum_ns;
    const uint32_t t3_quanta = (t3_ns + quantum_ns / 2) / quantum_ns;

    // Calculate SPI frequency: 1 / quantum_time
    // freq = 1 / (quantum_ns * 1e-9) = 1e9 / quantum_ns
    const uint32_t spi_freq_hz = 1000000000UL / quantum_ns;

    // Total bits per LED bit (symmetric for both bit 0 and bit 1)
    const uint32_t bits_per_led_bit = t1_quanta + t2_quanta + t3_quanta;

    // Validate maximum bit pattern length (32 bits max for storage)
    if (bits_per_led_bit > 32) {
        FL_WARN_ONCE("ChannelEngineSpi: Bit pattern too long ("
                << bits_per_led_bit
                << " bits), timing may not be achievable with SPI");
        return SpiTimingConfig::ws2812(
            chipsetTiming.reset_us); // Fall back to WS2812
    }

    // Generate bit patterns using 3-phase symmetric timing
    // Bit pattern: MSB first, 1 = high, 0 = low
    //
    // Bit '0': high for T1 quanta, low for T2+T3 quanta
    // Bit '1': high for T1+T2 quanta, low for T3 quanta

    uint32_t bit0_pattern = 0;
    uint32_t bit1_pattern = 0;

    // Set high bits for bit0 (T1 quanta high, T2+T3 low)
    for (uint32_t i = 0; i < t1_quanta; i++) {
        bit0_pattern |= (1UL << (bits_per_led_bit - 1 - i));
    }

    // Set high bits for bit1 (T1+T2 quanta high, T3 low)
    for (uint32_t i = 0; i < (t1_quanta + t2_quanta); i++) {
        bit1_pattern |= (1UL << (bits_per_led_bit - 1 - i));
    }

    // Calculate actual achieved timing (for validation/debugging)
    const uint32_t ns_per_bit = quantum_ns;
    const uint32_t achieved_t0h_ns =
        t1_quanta * ns_per_bit; // Bit '0' high time = T1
    const uint32_t achieved_t0l_ns =
        (t2_quanta + t3_quanta) * ns_per_bit; // Bit '0' low time = T2+T3
    const uint32_t achieved_t1h_ns =
        (t1_quanta + t2_quanta) * ns_per_bit; // Bit '1' high time = T1+T2
    const uint32_t achieved_t1l_ns =
        t3_quanta * ns_per_bit; // Bit '1' low time = T3

    // Construct SpiTimingConfig
    SpiTimingConfig config;
    config.protocol = SpiTimingConfig::CUSTOM;
    config.clock_hz = spi_freq_hz;
    config.bits_per_led_bit = static_cast<uint8_t>(bits_per_led_bit);
    config.reset_time_us = chipsetTiming.reset_us;
    config.bit0_pattern = bit0_pattern;
    config.bit0_count = static_cast<uint8_t>(bits_per_led_bit);
    config.bit1_pattern = bit1_pattern;
    config.bit1_count = static_cast<uint8_t>(bits_per_led_bit);
    config.achieved_t0h_ns = achieved_t0h_ns;
    config.achieved_t0l_ns = achieved_t0l_ns;
    config.achieved_t1h_ns = achieved_t1h_ns;
    config.achieved_t1l_ns = achieved_t1l_ns;

    return config;
}

SpiTimingConfig
ChannelEngineSpi::getSpiTimingFromChannel(const ChannelDataPtr &data) {
    const auto &chipsetTiming = data->getTiming();

    // Calculate optimal SPI timing from chipset specification
    SpiTimingConfig spiTiming = calculateSpiTiming(chipsetTiming);

    return spiTiming;
}

void ChannelEngineSpi::processPendingChannels() {
    // Try to process pending channels now that hardware may be available
    fl::vector_inlined<PendingChannel, 16> stillPending;

    for (auto &pending : mPendingChannels) {
        gpio_num_t pin = static_cast<gpio_num_t>(pending.pin);
        const auto &ledData = pending.data->getData();

        // Get original chipset timing for wave8 LUT precision
        const ChipsetTimingConfig& originalTiming = pending.data->getTiming();

        SpiChannelState *channel =
            acquireChannel(pin, pending.timing, ledData.size(), originalTiming);
        if (!channel) {
            // Still no hardware - increment retry count
            pending.retry_count++;

            // Give up after 50 failed attempts to prevent infinite retry storms
            if (pending.retry_count > 50) {
                FL_WARN_ONCE("ChannelEngineSpi: Giving up on pending channel for pin "
                         << pin << " after " << pending.retry_count << " failed attempts. "
                         << "Possible resource leak or hardware unavailability.");
                // Drop this pending channel (don't add to stillPending)
                continue;
            }

            // Keep pending for next poll()
            stillPending.push_back(pending);
            continue;
        }

        // Mark data as in-use (prevents strip driver from modifying during TX)
        pending.data->setInUse(true);

        // Successfully acquired channel - initialize streaming
        channel->stagingOffset = 0;
        channel->currentStaging = channel->stagingA;
        channel->transAInFlight = false;
        channel->transBInFlight = false;
        channel->transmissionComplete = false;

        // CRITICAL: Copy LED data to internal SRAM buffer for ISR-safe access
        if (channel->ledSourceBuffer && channel->ledSourceBufferSize >= ledData.size()) {
            fl::memcpy(channel->ledSourceBuffer, ledData.data(), ledData.size());
            channel->ledSource = channel->ledSourceBuffer;
        } else {
            channel->ledSource = ledData.data();
            FL_WARN_ONCE("ChannelEngineSpi: Using direct PSRAM access (ISR-unsafe!) in processPendingChannels");
        }
        channel->ledBytesRemaining = ledData.size();

        // Store reference to source data for cleanup
        channel->sourceData = pending.data;

        // Pre-encode ALL LED data
        preEncodeAllData(channel);

        // Kick-start the timer ISR for async transmission
        channel->hasNewData = true;
    }

    // Replace pending list with those that couldn't be processed
    mPendingChannels = fl::move(stillPending);
}

void ChannelEngineSpi::preEncodeAllData(SpiChannelState* channel) {
    // =========================================================================
    // Pre-encode ALL LED data into staging buffer
    // This function runs in MAIN TASK context (not ISR)
    // =========================================================================

    if (!channel || !channel->ledSource || channel->ledBytesRemaining == 0) {
        return;
    }

    const Wave8BitExpansionLut& lut = channel->mWave8Lut;
    size_t total_led_bytes = channel->ledBytesRemaining;
    size_t total_bytes_written = 0;

    // Reset staging to start fresh
    channel->stagingOffset = 0;
    channel->currentStaging = channel->stagingA;

    // Encode ALL LED data into staging buffer
    // For wave8 encoding: each LED byte expands to 8 SPI bytes
    // With 4KB staging buffer, we can hold ~512 LED bytes (1536 RGB LEDs worth of one color)
    // For typical use cases, this is sufficient. For larger strips, we fill what fits.

    uint8_t* output = channel->currentStaging;
    const size_t max_output = channel->stagingCapacity;

    if (channel->numLanes == 1) {
        // Single-lane encoding - each LED byte → 8 output bytes
        const size_t max_led_bytes = max_output / 8;
        const size_t bytes_to_encode = fl_min(total_led_bytes, max_led_bytes);

        for (size_t i = 0; i < bytes_to_encode; i++) {
            uint8_t input_byte = channel->ledSource[i];
            ::fl::Wave8Byte wave8_output;
            ::fl::detail::wave8_convert_byte_to_wave8byte(input_byte, lut, &wave8_output);
            fl::memcpy(output + total_bytes_written, &wave8_output, sizeof(::fl::Wave8Byte));
            total_bytes_written += 8;
        }

        channel->ledSource += bytes_to_encode;
        channel->ledBytesRemaining -= bytes_to_encode;

    } else if (channel->numLanes == 2) {
        // Dual-lane encoding - each 2 LED bytes → 16 output bytes
        const size_t max_led_bytes = (max_output / 16) * 2;
        size_t bytes_to_encode = fl_min(total_led_bytes, max_led_bytes);
        bytes_to_encode = (bytes_to_encode / 2) * 2; // Align to pairs

        for (size_t i = 0; i < bytes_to_encode / 2; i++) {
            const uint8_t lane0_byte = channel->ledSource[i * 2];
            const uint8_t lane1_byte = channel->ledSource[i * 2 + 1];

            ::fl::Wave8Byte wave8_lane0, wave8_lane1;
            ::fl::detail::wave8_convert_byte_to_wave8byte(lane0_byte, lut, &wave8_lane0);
            ::fl::detail::wave8_convert_byte_to_wave8byte(lane1_byte, lut, &wave8_lane1);

            ::fl::Wave8Byte lane_array[2] = {wave8_lane0, wave8_lane1};
            uint8_t transposed[16];
            ::fl::detail::wave8_transpose_2(lane_array, transposed);

            fl::memcpy(output + total_bytes_written, transposed, 16);
            total_bytes_written += 16;
        }

        channel->ledSource += bytes_to_encode;
        channel->ledBytesRemaining -= bytes_to_encode;

    } else if (channel->numLanes == 4) {
        // Quad-lane encoding - each 4 LED bytes → 32 output bytes
        const size_t max_led_bytes = (max_output / 32) * 4;
        size_t bytes_to_encode = fl_min(total_led_bytes, max_led_bytes);
        bytes_to_encode = (bytes_to_encode / 4) * 4; // Align to quads

        for (size_t i = 0; i < bytes_to_encode / 4; i++) {
            const uint8_t lane0_byte = channel->ledSource[i * 4];
            const uint8_t lane1_byte = channel->ledSource[i * 4 + 1];
            const uint8_t lane2_byte = channel->ledSource[i * 4 + 2];
            const uint8_t lane3_byte = channel->ledSource[i * 4 + 3];

            ::fl::Wave8Byte wave8_lanes[4];
            ::fl::detail::wave8_convert_byte_to_wave8byte(lane0_byte, lut, &wave8_lanes[0]);
            ::fl::detail::wave8_convert_byte_to_wave8byte(lane1_byte, lut, &wave8_lanes[1]);
            ::fl::detail::wave8_convert_byte_to_wave8byte(lane2_byte, lut, &wave8_lanes[2]);
            ::fl::detail::wave8_convert_byte_to_wave8byte(lane3_byte, lut, &wave8_lanes[3]);

            uint8_t transposed[32];
            ::fl::detail::wave8_transpose_4(wave8_lanes, transposed);

            fl::memcpy(output + total_bytes_written, transposed, 32);
            total_bytes_written += 32;
        }

        channel->ledSource += bytes_to_encode;
        channel->ledBytesRemaining -= bytes_to_encode;
    }

    channel->stagingOffset = total_bytes_written;

    // =========================================================================
    // ESP32-S3 CACHE COHERENCE (Iteration 12):
    // =========================================================================
    // ITERATION 12 FIX: Removed esp_cache_msync() calls entirely.
    //
    // Following Espressif's led_strip driver pattern:
    // - Staging buffers are allocated with MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA
    // - Internal SRAM on ESP32-S3 is non-cacheable for DMA operations
    // - Memory barriers alone are sufficient for ordering guarantees
    //
    // Previous iterations tried esp_cache_msync() which failed with
    // ESP_ERR_INVALID_ARG because internal DMA memory doesn't require
    // (or support) explicit cache synchronization operations.
    //
    // References:
    // - Espressif led_strip_spi_dev.c: Uses MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA
    //   without any esp_cache_msync() calls
    // - ESP-IDF cache API: Internal SRAM regions may not support cache ops
    // =========================================================================

    // Memory barrier: Ensure all CPU writes complete before DMA reads
    FL_MEMORY_BARRIER;

    FL_DBG_EVERY(100, "ChannelEngineSpi: Pre-encoded " << (total_led_bytes - channel->ledBytesRemaining)
           << " LED bytes → " << total_bytes_written << " SPI bytes");
}

void ChannelEngineSpi::transmitBlockingPolled(SpiChannelState* channel) {
    // =========================================================================
    // BLOCKING POLLED TRANSMIT (Iteration 12 - Simplified)
    // =========================================================================
    // Transmits pre-encoded LED data from staging buffer via SPI DMA.
    //
    // NOTE (Iteration 12): Cache coherence is now handled at allocation time
    // using MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA. No explicit cache sync needed.
    // The spi_bus_dma_memory_alloc() for the DMA buffer also handles alignment.
    // =========================================================================

    if (!channel || !channel->currentStaging || channel->stagingOffset == 0) {
        FL_WARN_EVERY(100, "ChannelEngineSpi: transmitBlockingPolled - no data to transmit");
        return;
    }

    const size_t bytes_to_send = channel->stagingOffset;

    FL_DBG_EVERY(100, "ChannelEngineSpi: transmitBlockingPolled - sending " << bytes_to_send << " bytes");

    // DEBUG: Print first 16 bytes of staging buffer BEFORE copy
    FL_DBG_EVERY(100, "ChannelEngineSpi: Staging (before copy) [0..15]: ["
           << static_cast<int>(channel->currentStaging[0]) << ","
           << static_cast<int>(channel->currentStaging[1]) << ","
           << static_cast<int>(channel->currentStaging[2]) << ","
           << static_cast<int>(channel->currentStaging[3]) << ","
           << static_cast<int>(channel->currentStaging[4]) << ","
           << static_cast<int>(channel->currentStaging[5]) << ","
           << static_cast<int>(channel->currentStaging[6]) << ","
           << static_cast<int>(channel->currentStaging[7]) << " | "
           << static_cast<int>(channel->currentStaging[8]) << ","
           << static_cast<int>(channel->currentStaging[9]) << ","
           << static_cast<int>(channel->currentStaging[10]) << ","
           << static_cast<int>(channel->currentStaging[11]) << ","
           << static_cast<int>(channel->currentStaging[12]) << ","
           << static_cast<int>(channel->currentStaging[13]) << ","
           << static_cast<int>(channel->currentStaging[14]) << ","
           << static_cast<int>(channel->currentStaging[15]) << "]");

    // =========================================================================
    // ITERATION 7: Use spi_bus_dma_memory_alloc for proper cache handling
    // =========================================================================
    // The ESP-IDF documentation states:
    //   "This API will take care of the cache and hardware alignment internally.
    //    To free/release memory allocated by this helper function, simply
    //    calling free()."
    //
    // Signature: void* spi_bus_dma_memory_alloc(host_id, size, extra_heap_caps)
    //
    // Previous attempts used heap_caps_calloc which may not have proper cache
    // attributes for ESP32-S3 DMA. The spi_bus_dma_memory_alloc function is
    // specifically designed for SPI DMA buffers.
    // =========================================================================
    uint8_t* dma_buf = static_cast<uint8_t*>(spi_bus_dma_memory_alloc(
        channel->spi_host, bytes_to_send, 0));

    if (!dma_buf) {
        FL_WARN_EVERY(100, "ChannelEngineSpi: spi_bus_dma_memory_alloc failed (" << bytes_to_send << " bytes)");
        channel->stagingOffset = 0;
        return;
    }

    // Copy encoded data from staging to DMA buffer
    fl::memcpy(dma_buf, channel->currentStaging, bytes_to_send);

    // Memory barrier to ensure all writes are visible
    FL_MEMORY_BARRIER;

    // DEBUG: Print first 16 bytes of DMA buffer AFTER copy
    FL_DBG_EVERY(100, "ChannelEngineSpi: DMA buf (after copy) [0..15]: ["
           << static_cast<int>(dma_buf[0]) << ","
           << static_cast<int>(dma_buf[1]) << ","
           << static_cast<int>(dma_buf[2]) << ","
           << static_cast<int>(dma_buf[3]) << ","
           << static_cast<int>(dma_buf[4]) << ","
           << static_cast<int>(dma_buf[5]) << ","
           << static_cast<int>(dma_buf[6]) << ","
           << static_cast<int>(dma_buf[7]) << " | "
           << static_cast<int>(dma_buf[8]) << ","
           << static_cast<int>(dma_buf[9]) << ","
           << static_cast<int>(dma_buf[10]) << ","
           << static_cast<int>(dma_buf[11]) << ","
           << static_cast<int>(dma_buf[12]) << ","
           << static_cast<int>(dma_buf[13]) << ","
           << static_cast<int>(dma_buf[14]) << ","
           << static_cast<int>(dma_buf[15]) << "]");

    // Setup transaction with the fresh DMA buffer
    spi_transaction_t trans = {};
    trans.length = bytes_to_send * 8;  // Convert to bits
    trans.tx_buffer = dma_buf;
    trans.rx_buffer = nullptr;
    trans.rxlength = 0;
    trans.user = nullptr;  // No callback needed for blocking transmit

    // DEBUG: Log transaction configuration
    FL_DBG_EVERY(100, "ChannelEngineSpi: Transaction config - length_bits=" << trans.length
           << " (" << (trans.length / 8) << " bytes)"
           << ", tx_buffer=" << fl::ptr_to_int(trans.tx_buffer)
           << ", host=" << static_cast<int>(channel->spi_host)
           << ", device=" << fl::ptr_to_int(channel->spi_device));

    // Set transaction mode based on lane count
    if (channel->numLanes >= 4) {
        trans.flags = SPI_TRANS_MODE_QIO;
    } else if (channel->numLanes >= 2) {
        trans.flags = SPI_TRANS_MODE_DIO;
    } else {
        trans.flags = 0;
    }

    // DEBUG: Capture first 8 bytes of tx_buffer for diagnostics
    if (bytes_to_send >= 8) {
        channel->debugTxCaptured = true;
        for (int i = 0; i < 8; i++) {
            channel->debugTxBuffer[i] = dma_buf[i];
        }
    }

    // =========================================================================
    // SPI TRANSMISSION DEBUG (Iteration 5)
    // =========================================================================
    uint32_t start_time = millis();

    // Acquire bus before transmission
    esp_err_t acq_ret = spi_device_acquire_bus(channel->spi_device, portMAX_DELAY);
    if (acq_ret != ESP_OK) {
        FL_WARN_EVERY(100, "ChannelEngineSpi: spi_device_acquire_bus failed: " << acq_ret);
        free(dma_buf);
        channel->stagingOffset = 0;
        return;
    }
    FL_DBG_EVERY(100, "ChannelEngineSpi: Bus acquired");

    // =========================================================================
    // ITERATION 14: RE-ADD PRE-TRANSMIT GPIO ROUTING
    // =========================================================================
    // The SPI driver may reconfigure GPIO during bus acquisition/release.
    // Re-apply GPIO matrix routing right before transmission to ensure
    // the MOSI signal is properly connected to the physical pin.
    //
    // This is called AFTER spi_device_acquire_bus() to ensure we don't
    // conflict with any GPIO setup the driver does during acquisition.
    // =========================================================================
    int mosi_signal = spi_periph_signal[channel->spi_host].spid_out;
    esp_rom_gpio_connect_out_signal(channel->pin, mosi_signal, false, false);
    gpio_set_direction(channel->pin, GPIO_MODE_OUTPUT);
    FL_DBG_EVERY(100, "ChannelEngineSpi: Pre-TX GPIO routing: signal " << mosi_signal << " → GPIO " << static_cast<int>(channel->pin));

    // Try spi_device_polling_transmit with bus acquired (this should use the hardware directly)
    esp_err_t ret = spi_device_polling_transmit(channel->spi_device, &trans);

    uint32_t elapsed_ms = millis() - start_time;

    // Release bus
    spi_device_release_bus(channel->spi_device);

    if (ret != ESP_OK) {
        FL_WARN_EVERY(100, "ChannelEngineSpi: spi_device_polling_transmit failed: " << ret);
    } else {
        FL_DBG_EVERY(100, "ChannelEngineSpi: Transmit complete, sent " << bytes_to_send << " bytes in " << elapsed_ms << "ms");

        // Expected time for 2400 bytes at 2.5MHz:
        // 2400 * 8 bits = 19200 bits
        // At 2.5MHz = 19200 / 2.5e6 = 7.68ms
        // If elapsed < 1ms, something is wrong
        if (elapsed_ms < 1 && bytes_to_send > 100) {
            FL_WARN_EVERY(100, "ChannelEngineSpi: Transmission too fast! Expected ~" << (bytes_to_send * 8 / 2500) << "ms, got " << elapsed_ms << "ms");
        }
    }

    // Free the temporary DMA buffer (allocated with spi_bus_dma_memory_alloc)
    free(dma_buf);

    // Clear staging offset (buffer consumed)
    channel->stagingOffset = 0;
}

void IRAM_ATTR
ChannelEngineSpi::spiPostTransactionCallback(spi_transaction_t *trans) {
    SpiChannelState *channel = static_cast<SpiChannelState *>(trans->user);

    // Safety check: if user pointer is NULL, this is not a managed transaction
    if (!channel) {
        return;
    }

    // Mark which transaction completed
    if (trans == &channel->transA) {
        channel->transAInFlight = false;
    } else if (trans == &channel->transB) {
        channel->transBInFlight = false;
    }

    // Arm timer ISR to start encoding next chunk
    channel->hasNewData = true;
}

void IRAM_ATTR ChannelEngineSpi::timerEncodingISR(void *user_data) {
    SpiChannelState *channel = static_cast<SpiChannelState *>(user_data);

    // CRITICAL: Validate channel pointer
    if (!channel) {
        return;
    }

    // CRITICAL: Check shutdown flag BEFORE accessing any channel resources
    if (channel->isShuttingDown) {
        return;
    }

    // Check if armed by transaction complete callback or initial kick
    if (!channel->hasNewData) {
        return; // No work, exit fast
    }

    // =========================================================================
    // SIMPLIFIED ISR: Only queue pre-encoded data (no encoding here!)
    // Encoding happens in preEncodeAllData() in main task context where
    // esp_cache_msync() can be safely called.
    // =========================================================================

    // Check if there's pre-encoded data to transmit
    if (channel->stagingOffset == 0) {
        // No data to transmit - disarm and wait
        channel->hasNewData = false;
        return;
    }

    // CRITICAL: Validate currentStaging pointer before use
    if (!channel->currentStaging) {
        channel->hasNewData = false;
        return;
    }

    // Select which transaction to use
    spi_transaction_t *trans =
        channel->transAInFlight ? &channel->transB : &channel->transA;

    // Don't queue if both transactions are in flight
    if (channel->transAInFlight && channel->transBInFlight) {
        // Both transactions busy - keep hasNewData=true and retry on next ISR fire
        return;
    }

    // Setup transaction with pre-encoded data
    trans->length = channel->stagingOffset * 8; // Convert to bits
    trans->tx_buffer = channel->currentStaging;
    trans->rx_buffer = NULL;
    trans->rxlength = 0;
    trans->user = channel;

    // Set transaction mode based on lane count
    if (channel->numLanes >= 4) {
        trans->flags = SPI_TRANS_MODE_QIO;
    } else if (channel->numLanes >= 2) {
        trans->flags = SPI_TRANS_MODE_DIO;
    } else {
        trans->flags = 0;
    }

    // DEBUG: Capture first 8 bytes of tx_buffer right before queuing
    if (!channel->debugTxCaptured && trans->tx_buffer && channel->stagingOffset >= 8) {
        channel->debugTxCaptured = true;
        const uint8_t* buf = static_cast<const uint8_t*>(trans->tx_buffer);
        for (int i = 0; i < 8; i++) {
            channel->debugTxBuffer[i] = buf[i];
        }
    }

    // Try to queue transaction (non-blocking in ISR)
    esp_err_t ret = spi_device_queue_trans(channel->spi_device, trans, 0);

    if (ret == ESP_OK) {
        // Transaction posted successfully
        if (trans == &channel->transA) {
            channel->transAInFlight = true;
        } else {
            channel->transBInFlight = true;
        }

        // Mark this buffer as transmitted (clear stagingOffset)
        // NOTE: Since we pre-encode ALL data in main task, we don't flip buffers here.
        // The staging buffer is consumed in one transaction.
        channel->stagingOffset = 0;

        // Disarm - transmission is done for this frame
        // (no more chunks to encode since we pre-encoded everything)
        channel->hasNewData = false;
    }
    // If queue full (ret != ESP_OK), keep hasNewData=true and retry on next ISR fire
}

} // namespace fl

#endif // FASTLED_ESP32_HAS_CLOCKLESS_SPI

#endif // ESP32
