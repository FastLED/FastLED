/// @file channel_engine_spi.cpp
/// @brief SPI-based ChannelEngine implementation for ESP32

#include "channel_engine_spi.h"

#ifdef ESP32

#include "platforms/esp/32/feature_flags/enabled.h"

#if FASTLED_ESP32_HAS_CLOCKLESS_SPI

#include "fl/dbg.h"
#include "fl/delay.h"
#include "fl/math_macros.h"
#include "fl/warn.h"
#include "fl/stl/time.h"
#include "fl/channels/detail/wave8.hpp" // Inline wave8 functions
#include "platforms/esp/32/drivers/spi/spi_hw_base.h" // SPI host definitions (SPI2_HOST, SPI3_HOST)
#include "platforms/esp/is_esp.h" // Platform detection (FL_IS_ESP_32C6, etc.)
#include "platforms/esp/32/drivers/spi/wave8_encoder_spi.h" // wave8 encoding (Stage 6)

FL_EXTERN_C_BEGIN
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_check.h"
#include "esp_heap_caps.h"
#include "soc/spi_periph.h"
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

        // Free staging buffers
        if (channel.stagingA) {
            heap_caps_free(channel.stagingA);
        }
        if (channel.stagingB) {
            heap_caps_free(channel.stagingB);
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

        // Check if streaming is complete
        // Complete when: ledBytesRemaining == 0 AND both transactions finished
        bool streaming_complete = (channel.ledBytesRemaining == 0) &&
                                  !channel.transAInFlight &&
                                  !channel.transBInFlight;

        if (streaming_complete && !channel.transmissionComplete) {
            // All encoding and transmission done
            channel.transmissionComplete = true;

            // Clear in-use flag on source data
            if (channel.sourceData) {
                channel.sourceData->setInUse(false);
            }

            releaseChannel(&channel);
        } else if (!streaming_complete) {
            // Still encoding/transmitting
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
        FL_WARN("ChannelEngineSpi: Pending queue not empty at batch start ("
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

    FL_DBG("ChannelEngineSpi: Grouped " << channels.size()
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

        FL_DBG("ChannelEngineSpi: Timing group with " << N << " channels, "
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
                    FL_WARN("ChannelEngineSpi: Error during batch transmission");
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
                FL_DBG("ChannelEngineSpi: Inserting reset delay ("
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

// Use FASTLED_ESP32_HAS_SPI3 (based on SOC_SPI_PERIPH_NUM) for accurate detection
// ESP32-C3/C6/H2: 1 usable SPI host (SPI2)
// ESP32/S2/S3/P4: 2 usable SPI hosts (SPI2, SPI3)
#if FASTLED_ESP32_HAS_SPI3
    constexpr uint8_t PARALLEL_SPI_HOSTS = 2;  // ESP32/S2/S3/P4
#elif defined(SPI2_HOST)
    constexpr uint8_t PARALLEL_SPI_HOSTS = 1;  // ESP32-C3/C6/H2
#else
    constexpr uint8_t PARALLEL_SPI_HOSTS = 0;  // No SPI available
#endif

    FL_DBG("ChannelEngineSpi: Determined lane capacity: "
           << static_cast<int>(PARALLEL_SPI_HOSTS) << " SPI hosts");
    return PARALLEL_SPI_HOSTS;
}

void ChannelEngineSpi::beginTransmission(
    fl::span<const ChannelDataPtr> channelData) {
    for (const auto &data : channelData) {
        gpio_num_t pin = static_cast<gpio_num_t>(data->getPin());

        // Get timing configuration from channel data
        SpiTimingConfig timing = getSpiTimingFromChannel(data);

        // Get LED data from channel
        const auto &ledData = data->getData();
        if (ledData.empty()) {
            FL_WARN("ChannelEngineSpi: Empty LED data for pin " << pin);
            continue;
        }

        // Acquire or reuse channel
        SpiChannelState *channel = acquireChannel(pin, timing, ledData.size());
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
        channel->ledSource = ledData.data();
        channel->ledBytesRemaining = ledData.size();
        channel->stagingOffset = 0;
        channel->currentStaging = channel->stagingA;
        channel->transAInFlight = false;
        channel->transBInFlight = false;
        channel->transmissionComplete = false;

        // Store reference to source data for cleanup
        channel->sourceData = data;

        // Kick-start the timer ISR by setting hasNewData flag
        // The timer ISR will start encoding chunks and posting transactions
        channel->hasNewData = true;
    }
}

ChannelEngineSpi::SpiChannelState *
ChannelEngineSpi::acquireChannel(gpio_num_t pin, const SpiTimingConfig &timing,
                                 size_t dataSize) {

    // Try to find existing idle channel with matching pin and timing
    for (auto &channel : mChannels) {
        if (!channel.inUse && channel.pin == pin && channel.timing == timing) {
            channel.inUse = true;
            channel.transmissionComplete = false;
            channel.hasNewData = false;
            channel.ledBytesRemaining = 0;
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
    if (!createChannel(channelPtr, pin, timing, dataSize)) {
        FL_WARN("ChannelEngineSpi: Failed to create channel for pin " << pin);
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

        // Clear transaction flags
        channel->transAInFlight = false;
        channel->transBInFlight = false;

        // Release reference to source data
        channel->sourceData = nullptr;
    }
}

bool ChannelEngineSpi::createChannel(SpiChannelState *state, gpio_num_t pin,
                                     const SpiTimingConfig &timing,
                                     size_t dataSize) {
    FL_DBG("ChannelEngineSpi: Creating channel for pin " << pin);

    // Acquire SPI host
    state->spi_host = acquireSpiHost();
    if (state->spi_host == SPI_HOST_MAX) {
        FL_WARN("ChannelEngineSpi: No available SPI host");
        return false;
    }

    // Calculate buffer size (variable expansion ratio based on timing)
    // Total bits = dataSize * 8 * timing.bits_per_led_bit
    // Convert to bytes (round up)
    const size_t total_bits = dataSize * 8 * timing.bits_per_led_bit;
    const size_t spiBufferSize =
        (total_bits + 7) / 8; // Round up to nearest byte

    // Determine if we should use DMA
    // Use DMA for larger buffers (>64 bytes) similar to Espressif driver
    state->useDMA = (spiBufferSize > 64);

    // Configure SPI bus with multi-lane support
    spi_bus_config_t bus_config = {};
    bus_config.mosi_io_num = pin; // Data0 (always present)
    bus_config.miso_io_num =
        state->data1_pin;        // Data1 for dual/quad mode (-1 if unused)
    // CRITICAL: SPI peripheral requires a clock signal for MOSI timing
    // Clock pin must be specified even though we don't physically connect it to LEDs
    // The clock determines the timing of MOSI bit transitions
    // For ESP32-S3, use GPIO 1 as internal clock (not connected to LED strip)
    // Note: GPIO 3 is a strapping pin (JTAG select) and should be avoided
    bus_config.sclk_io_num = 1;  // Internal clock pin (required for SPI timing)
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
    // Always use SPI_DMA_CH_AUTO - ESP-IDF will handle DMA allocation
    // efficiently ESP32-S3 and newer chips require auto-allocation (don't
    // support SPI_DMA_DISABLED) Older chips work fine with auto-allocation too
    // (driver optimizes internally)
    esp_err_t ret =
        spi_bus_initialize(state->spi_host, &bus_config, SPI_DMA_CH_AUTO);
    if (ret != ESP_OK) {
        FL_WARN("ChannelEngineSpi: spi_bus_initialize failed: " << ret);
        releaseSpiHost(state->spi_host);
        state->spi_host = SPI_HOST_MAX;
        return false;
    }

    // Configure SPI device
    spi_device_interface_config_t dev_config = {};
    dev_config.command_bits = 0;
    dev_config.address_bits = 0;
    dev_config.dummy_bits = 0;
    dev_config.clock_speed_hz = timing.clock_hz;
    dev_config.mode = 0;          // SPI mode 0
    dev_config.spics_io_num = -1; // No CS pin
    dev_config.queue_size = 4;    // Transaction queue size
    dev_config.post_cb =
        spiPostTransactionCallback; // ISR callback when transaction completes

    // Set HALFDUPLEX flag for multi-lane modes (required for dual/quad SPI)
    if (state->numLanes >= 2) {
        dev_config.flags = SPI_DEVICE_HALFDUPLEX;
    } else {
        dev_config.flags = 0;
    }

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
    if (actual_freq_khz < requested_freq_khz - 300 ||
        actual_freq_khz > requested_freq_khz + 300) {
        FL_WARN("ChannelEngineSpi: Clock frequency mismatch - requested "
                << requested_freq_khz << " kHz, actual " << actual_freq_khz
                << " kHz");
    }

    // Allocate double-buffered staging buffers (4KB each, DMA-capable)
    const size_t staging_size = 4096;

    state->stagingA = (uint8_t *)heap_caps_aligned_alloc(
        4, staging_size, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
    state->stagingB = (uint8_t *)heap_caps_aligned_alloc(
        4, staging_size, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);

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

    // CRITICAL: Zero the staging buffers - encodeLedByte() uses |= which
    // assumes zero-initialized memory
    fl::memset(state->stagingA, 0, staging_size);
    fl::memset(state->stagingB, 0, staging_size);

    state->stagingCapacity = staging_size;
    state->currentStaging = state->stagingA;
    state->stagingOffset = 0;

    // Initialize streaming state
    state->hasNewData = false;
    state->ledSource = nullptr;
    state->ledBytesRemaining = 0;
    state->transAInFlight = false;
    state->transBInFlight = false;

    // Build wave8 expansion LUT once during channel creation (not in ISR)
    ChipsetTiming chipsetTiming = convertSpiTimingToChipsetTiming(timing);
    state->mWave8Lut = buildWave8ExpansionLUT(chipsetTiming);
    FL_DBG("ChannelEngineSpi: Initialized wave8 LUT for channel");

    // Set up timer ISR (4 kHz = 250μs period)
    fl::isr::isr_config_t isr_config;
    isr_config.handler = timerEncodingISR;
    isr_config.user_data = state;
    isr_config.frequency_hz = 4000; // 4 kHz
    isr_config.flags = 0;           // Auto-reload

    fl::isr::isr_handle_t isr_handle;
    int isr_ret = fl::isr::attachTimerHandler(isr_config, &isr_handle);
    if (isr_ret != 0) {
        FL_WARN("ChannelEngineSpi: Failed to attach timer ISR: " << isr_ret);
        heap_caps_free(state->stagingA);
        heap_caps_free(state->stagingB);
        spi_bus_remove_device(state->spi_device);
        spi_bus_free(state->spi_host);
        releaseSpiHost(state->spi_host);
        return false;
    }

    state->timerHandle = isr_handle;

    FL_DBG("ChannelEngineSpi: Channel created successfully - pin="
           << pin << ", lanes=" << static_cast<int>(state->numLanes)
           << " (data0=" << pin << ", data1=" << state->data1_pin << ", data2="
           << state->data2_pin << ", data3=" << state->data3_pin << ")"
           << ", host=" << state->spi_host << ", dma=" << state->useDMA
           << ", freq=" << timing.clock_hz << " Hz"
           << ", expansion=" << static_cast<int>(timing.bits_per_led_bit) << "x"
           << ", staging=" << staging_size << " bytes × 2");

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
    // SPI host priority: SPI2 → SPI3 → SPI1
    // Note: SPI1 is often used for flash, so it's last priority
    // Use FASTLED_ESP32_HAS_SPI3 (based on SOC_SPI_PERIPH_NUM) for accurate detection
    static const spi_host_device_t hosts[] = {
#ifdef SPI2_HOST
        SPI2_HOST,
#endif
#if FASTLED_ESP32_HAS_SPI3
        SPI3_HOST,
#endif
#ifdef SPI1_HOST
        SPI1_HOST,
#endif
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

    FL_WARN("ChannelEngineSpi: No available SPI hosts (max " << num_hosts
                                                             << " hosts)");
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
        FL_WARN("ChannelEngineSpi: Quantum too small ("
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
        FL_WARN("ChannelEngineSpi: Bit pattern too long ("
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

        SpiChannelState *channel =
            acquireChannel(pin, pending.timing, ledData.size());
        if (!channel) {
            // Still no hardware - keep pending
            stillPending.push_back(pending);
            continue;
        }

        // Mark data as in-use (prevents strip driver from modifying during TX)
        pending.data->setInUse(true);

        // Successfully acquired channel - initialize streaming
        channel->ledSource = ledData.data();
        channel->ledBytesRemaining = ledData.size();
        channel->stagingOffset = 0;
        channel->currentStaging = channel->stagingA;
        channel->transAInFlight = false;
        channel->transBInFlight = false;
        channel->transmissionComplete = false;

        // Store reference to source data for cleanup
        channel->sourceData = pending.data;

        // Kick-start the timer ISR
        channel->hasNewData = true;
    }

    // Replace pending list with those that couldn't be processed
    mPendingChannels = fl::move(stillPending);
}

void IRAM_ATTR
ChannelEngineSpi::spiPostTransactionCallback(spi_transaction_t *trans) {
    SpiChannelState *channel = static_cast<SpiChannelState *>(trans->user);

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

    // CRITICAL: Validate channel pointer (should never be NULL, but check for
    // corruption)
    if (!channel) {
        // Silently return - cannot log in ISR context
        return;
    }

    // Check if armed by transaction complete callback
    if (!channel->hasNewData) {
        return; // No work, exit fast
    }

    // Check if there's data left to encode
    if (channel->ledBytesRemaining == 0) {
        channel->hasNewData = false;
        // NOTE: Do NOT set transmissionComplete here - the poll() function will
        // detect completion when ledBytesRemaining==0 AND all transactions finish.
        return; // All encoding done
    }

    // CRITICAL: Validate currentStaging pointer before use
    if (!channel->currentStaging) {
        // Silently disarm - cannot log in ISR context
        channel->hasNewData = false;
        return;
    }

    // Additional sanity check: verify currentStaging points to one of the
    // staging buffers
    if (channel->currentStaging != channel->stagingA &&
        channel->currentStaging != channel->stagingB) {
        // Attempt recovery: reset to stagingA (silently - cannot log in ISR context)
        channel->currentStaging = channel->stagingA;
        channel->stagingOffset = 0;
        channel->hasNewData = false;
        return;
    }

    // Encode one chunk with lane-scaled throughput
    // Single-lane: 40 LEDs per ISR fire (250μs period)
    // Dual-lane: 80 LEDs (2x throughput, 2x encoding needed)
    // Quad-lane: 160 LEDs (4x throughput, 4x encoding needed)
    const size_t base_chunk_leds = 40;
    const size_t chunk_size_leds = base_chunk_leds * channel->numLanes;
    const size_t chunk_size_bytes = chunk_size_leds * 3; // RGB

    size_t bytes_to_encode =
        fl_min(chunk_size_bytes, channel->ledBytesRemaining);

    // Calculate worst-case output size (expansion ratio from timing config)
    // Each LED byte expands to timing.bits_per_led_bit * 8 bits
    const size_t bits_per_led_byte = channel->timing.bits_per_led_bit * 8;
    const size_t worst_case_output_bits = bytes_to_encode * bits_per_led_byte;
    const size_t worst_case_output_bytes = (worst_case_output_bits + 7) / 8;
    const size_t available_space =
        channel->stagingCapacity - channel->stagingOffset;

    // CRITICAL: Check buffer overflow BEFORE encoding
    if (worst_case_output_bytes > available_space) {
        // Not enough space - reduce chunk size to fit
        const size_t max_input_bytes =
            (available_space * 8) / bits_per_led_byte;
        bytes_to_encode = fl_min(bytes_to_encode, max_input_bytes);

        // If buffer is full, encoding will proceed with 0 bytes (no-op)
        // Cannot log in ISR context
    }

    // Encode into staging buffer at current offset using wave8 encoding (fast, lookup-based)

    // Use cached LUT (built once during channel creation)
    const Wave8BitExpansionLut& lut = channel->mWave8Lut;

    // Create span wrappers for input/output
    fl::span<const uint8_t> input_span(channel->ledSource, bytes_to_encode);
    uint8_t* output = channel->currentStaging + channel->stagingOffset;
    size_t output_capacity = channel->stagingCapacity - channel->stagingOffset;
    fl::span<uint8_t> output_span(output, output_capacity);

    // Encode based on lane count
    size_t bytes_written = 0;
    if (channel->numLanes == 1) {
        // Single-lane: No transposition (optimized for ESP32-C3)
        bytes_written = wave8EncodeSingleLane(
            input_span,
            output_span,
            lut
        );
    } else if (channel->numLanes == 2) {
        // Dual-lane: 2-lane transposition
        // Data is interleaved: [B0, B1, B2, B3, ...] where Lane0=[B0,B2,...], Lane1=[B1,B3,...]

        // Calculate bytes per lane (input must be even number of bytes)
        const size_t bytes_per_lane = bytes_to_encode / 2;
        if (bytes_to_encode % 2 != 0) {
            // Odd byte count - encode the extra byte separately (should be rare)
            // This ensures we don't read past buffer bounds
            bytes_to_encode--; // Process one less byte, leave odd byte for next iteration
        }

        // Validate output buffer size (16 bytes per input byte pair = 8x expansion * 2 lanes)
        const size_t required_output = bytes_per_lane * 16;
        if (output_capacity < required_output) {
            // Buffer overflow protection - reduce chunk size
            bytes_written = 0;
        } else {
            // Manual dual-lane encoding with interleaved input
            size_t output_idx = 0;
            for (size_t i = 0; i < bytes_per_lane; i++) {
                // Extract bytes from interleaved input
                const uint8_t lane0_byte = channel->ledSource[i * 2];     // Even indices
                const uint8_t lane1_byte = channel->ledSource[i * 2 + 1]; // Odd indices

                // Convert both lane bytes to Wave8Byte
                ::fl::Wave8Byte wave8_lane0, wave8_lane1;
                ::fl::detail::wave8_convert_byte_to_wave8byte(lane0_byte, lut, &wave8_lane0);
                ::fl::detail::wave8_convert_byte_to_wave8byte(lane1_byte, lut, &wave8_lane1);

                // Transpose 2 lanes into interleaved format
                ::fl::Wave8Byte lane_array[2] = {wave8_lane0, wave8_lane1};
                uint8_t transposed[2 * sizeof(::fl::Wave8Byte)]; // 16 bytes
                ::fl::detail::wave8_transpose_2(lane_array, transposed);

                // Copy transposed data to output buffer
                ::fl::memcpy(output + output_idx, transposed, sizeof(transposed));
                output_idx += sizeof(transposed);
            }
            bytes_written = output_idx;
        }
    } else if (channel->numLanes == 4) {
        // Quad-lane: 4-lane transposition
        // Data is interleaved: [B0, B1, B2, B3, B4, ...] where Lane0=[B0,B4,...], Lane1=[B1,B5,...], etc.

        // Calculate bytes per lane (input must be multiple of 4)
        const size_t bytes_per_lane = bytes_to_encode / 4;
        const size_t aligned_bytes = bytes_per_lane * 4;
        if (bytes_to_encode != aligned_bytes) {
            // Not aligned to 4-byte boundary - process only aligned portion
            bytes_to_encode = aligned_bytes;
        }

        // Validate output buffer size (32 bytes per 4-byte input = 8x expansion * 4 lanes)
        const size_t required_output = bytes_per_lane * 32;
        if (output_capacity < required_output) {
            // Buffer overflow protection - reduce chunk size
            bytes_written = 0;
        } else {
            // Manual quad-lane encoding with interleaved input
            size_t output_idx = 0;
            for (size_t i = 0; i < bytes_per_lane; i++) {
                // Extract bytes from interleaved input
                const uint8_t lane0_byte = channel->ledSource[i * 4];     // Indices 0, 4, 8, ...
                const uint8_t lane1_byte = channel->ledSource[i * 4 + 1]; // Indices 1, 5, 9, ...
                const uint8_t lane2_byte = channel->ledSource[i * 4 + 2]; // Indices 2, 6, 10, ...
                const uint8_t lane3_byte = channel->ledSource[i * 4 + 3]; // Indices 3, 7, 11, ...

                // Convert all 4 lane bytes to Wave8Byte
                ::fl::Wave8Byte wave8_lanes[4];
                ::fl::detail::wave8_convert_byte_to_wave8byte(lane0_byte, lut, &wave8_lanes[0]);
                ::fl::detail::wave8_convert_byte_to_wave8byte(lane1_byte, lut, &wave8_lanes[1]);
                ::fl::detail::wave8_convert_byte_to_wave8byte(lane2_byte, lut, &wave8_lanes[2]);
                ::fl::detail::wave8_convert_byte_to_wave8byte(lane3_byte, lut, &wave8_lanes[3]);

                // Transpose 4 lanes into interleaved format
                uint8_t transposed[4 * sizeof(::fl::Wave8Byte)]; // 32 bytes
                ::fl::detail::wave8_transpose_4(wave8_lanes, transposed);

                // Copy transposed data to output buffer
                ::fl::memcpy(output + output_idx, transposed, sizeof(transposed));
                output_idx += sizeof(transposed);
            }
            bytes_written = output_idx;
        }
    }

    // Update staging buffer position
    channel->stagingOffset += bytes_written;

    // Update source data position
    channel->ledSource += bytes_to_encode;
    channel->ledBytesRemaining -= bytes_to_encode;

    // Check if staging buffer is full or nearly full or this is the last chunk
    // CRITICAL: Use threshold instead of exact capacity to prevent edge case where
    // buffer has a few spare bytes but not enough for next chunk (causing deadlock).
    // With 10x expansion, typical chunk is ~150 bytes, so 200-byte margin is safe.
    const size_t buffer_threshold = channel->stagingCapacity - 200;
    bool buffer_full = (channel->stagingOffset >= buffer_threshold);
    bool last_chunk = (channel->ledBytesRemaining == 0);

    if (buffer_full || last_chunk) {
        // Select which transaction to use
        spi_transaction_t *trans =
            channel->transAInFlight ? &channel->transB : &channel->transA;

        // Setup transaction with multi-lane flags
        trans->length = channel->stagingOffset * 8; // Convert to bits
        trans->tx_buffer = channel->currentStaging;
        trans->rx_buffer = NULL; // Output-only, no receive
        trans->rxlength = 0;     // Output-only, no receive length
        trans->user = channel;

        // Set transaction mode based on lane count (1, 2, or 4 lanes)
        // NOTE: Use output-only modes (not bidirectional DIO/QIO)
        if (channel->numLanes >= 4) {
            trans->flags = SPI_TRANS_MODE_QIO; // Quad output mode
        } else if (channel->numLanes >= 2) {
            trans->flags = SPI_TRANS_MODE_DIO; // Dual output mode
        } else {
            trans->flags = 0; // Standard SPI mode
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

            // Flip to other staging buffer for next accumulation
            channel->currentStaging =
                (channel->currentStaging == channel->stagingA)
                    ? channel->stagingB
                    : channel->stagingA;
            channel->stagingOffset = 0;

            // Disarm until next transaction completes
            channel->hasNewData = false;
        }
        // If queue full (ret != ESP_OK), keep hasNewData=true and retry next
        // ISR fire
    }
}

} // namespace fl

#endif // FASTLED_ESP32_HAS_CLOCKLESS_SPI

#endif // ESP32
