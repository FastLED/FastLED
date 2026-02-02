/// @file channel_engine_spi.cpp
/// @brief Clockless-over-SPI ChannelEngine implementation for ESP32
///
/// ⚠️ ARCHITECTURE NOTE: This is NOT a general SPI LED driver!
/// This driver implements CLOCKLESS protocols (WS2812, SK6812, etc.) using SPI hardware
/// as a bit-banging engine. The SPI clock is used internally for timing but is NEVER
/// physically connected to the LED strip - only the MOSI/data pin is used.
/// See channel_engine_spi.h for detailed explanation.
///
/// ENCODING: Uses Espressif led_strip 3-bit encoding (NOT wave8):
/// - LED bit '0' → SPI bits: 100 (binary) = short high, long low
/// - LED bit '1' → SPI bits: 110 (binary) = long high, short low
/// - Each LED byte (8 bits) expands to 3 SPI bytes (24 bits)
/// - SPI clock: 2.5 MHz (400ns per bit)
/// Reference: https://github.com/espressif/idf-extra-components/blob/master/led_strip/src/led_strip_spi_dev.c
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
#include "fl/dbg.h"
#include "fl/delay.h"
#include "fl/math_macros.h"
#include "fl/warn.h"
#include "fl/stl/time.h"
#include "platforms/esp/32/drivers/spi/spi_hw_base.h" // SPI host definitions (SPI2_HOST, SPI3_HOST)
#include "platforms/esp/is_esp.h" // Platform detection (FL_IS_ESP_32C6, etc.)
#include "platforms/memory_barrier.h" // FL_MEMORY_BARRIER for cache coherence

FL_EXTERN_C_BEGIN
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_check.h"
#include "esp_heap_caps.h"
#include "soc/spi_periph.h"
#include "soc/spi_reg.h"    // SPI_DMA_CONF_REG for ESP32-S3 DMA fix
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

// ============================================================================
// Espressif 3-bit LED Encoding (matches led_strip_spi_dev.c)
// ============================================================================
// Reference: https://github.com/espressif/idf-extra-components/blob/master/led_strip/src/led_strip_spi_dev.c
//
// Encoding scheme:
// - Each LED bit expands to 3 SPI bits
// - LED bit '0' → SPI bits: 100 (binary) = 0x4 (high nibble position)
// - LED bit '1' → SPI bits: 110 (binary) = 0x6 (high nibble position)
// - Each LED color byte (8 bits) expands to 24 SPI bits (3 bytes)
// - SPI clock: 2.5 MHz = 400ns per SPI bit
//
// Timing achieved:
// - LED '0': T0H = 400ns (1 SPI bit), T0L = 800ns (2 SPI bits)
// - LED '1': T1H = 800ns (2 SPI bits), T1L = 400ns (1 SPI bit)

/// @brief Number of SPI bytes per LED color byte (3:1 expansion)
constexpr size_t SPI_BYTES_PER_COLOR_BYTE = 3;

/// @brief Encode a single LED color byte into 3 SPI bytes
/// @param data The LED color value (0-255)
/// @param buf Pointer to 3-byte output buffer (must be pre-cleared)
///
/// EXACT copy of Espressif led_strip encoding function:
/// https://github.com/espressif/idf-extra-components/blob/master/led_strip/src/led_strip_spi_dev.c
///
/// Each color bit (8 bits) is represented by 3 SPI bits:
///   - low_level (LED '0'): 100 (binary)
///   - high_level (LED '1'): 110 (binary)
/// So a color byte occupies 3 bytes of SPI data.
///
/// Bit mapping (buf is treated as little-endian within each byte):
///   Input bit 0 → buf[2] bits 0-2 (3 bits starting at LSB)
///   Input bit 1 → buf[2] bits 3-5
///   Input bit 2 → buf[2] bit 6-7 + buf[1] bit 0
///   Input bit 3 → buf[1] bits 1-3
///   Input bit 4 → buf[1] bits 4-6
///   Input bit 5 → buf[1] bit 7 + buf[0] bits 0-1
///   Input bit 6 → buf[0] bits 2-4
///   Input bit 7 → buf[0] bits 5-7
FASTLED_FORCE_INLINE
void led_strip_encode_byte(uint8_t data, uint8_t* buf) {
    // Exact Espressif implementation - DO NOT MODIFY
    // Each color of 1 bit is represented by 3 bits of SPI,
    // low_level:100, high_level:110
    // So a color byte occupies 3 bytes of SPI.
    *(buf + 2) |= data & (1 << 0) ? (1 << 2) | (1 << 1) : (1 << 2);
    *(buf + 2) |= data & (1 << 1) ? (1 << 5) | (1 << 4) : (1 << 5);
    *(buf + 2) |= data & (1 << 2) ? (1 << 7) : 0x00;
    *(buf + 1) |= (1 << 0);
    *(buf + 1) |= data & (1 << 3) ? (1 << 3) | (1 << 2) : (1 << 3);
    *(buf + 1) |= data & (1 << 4) ? (1 << 6) | (1 << 5) : (1 << 6);
    *(buf + 0) |= data & (1 << 5) ? (1 << 1) | (1 << 0) : (1 << 1);
    *(buf + 0) |= data & (1 << 6) ? (1 << 4) | (1 << 3) : (1 << 4);
    *(buf + 0) |= data & (1 << 7) ? (1 << 7) | (1 << 6) : (1 << 7);
}

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

        // With blocking transmit, transmission is complete as soon as
        // beginTransmission() returns. Check transmissionComplete flag.
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

        // Get original chipset timing for reference (not used for encoding, kept for debug)
        const ChipsetTimingConfig& originalTiming = data->getTiming();

        // DEBUG: Trace timing values received from channel data
        FL_DBG_EVERY(100, "ChannelEngineSpi: Received timing from ChannelData: t1_ns="
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

            // DEBUG: Verify Espressif 3-bit encoding works by testing first byte
            if (channel->ledSourceBuffer && ledData.size() >= 1) {
                uint8_t test_byte = channel->ledSourceBuffer[0];
                uint8_t test_output[SPI_BYTES_PER_COLOR_BYTE] = {0, 0, 0};  // Must be pre-cleared
                led_strip_encode_byte(test_byte, test_output);
                FL_DBG_EVERY(100, "ChannelEngineSpi: Test encode byte " << static_cast<int>(test_byte)
                       << " → SPI[0..2]: [" << static_cast<int>(test_output[0])
                       << "," << static_cast<int>(test_output[1])
                       << "," << static_cast<int>(test_output[2]) << "]");
            }
        }

        // Store reference to source data for cleanup
        channel->sourceData = data;

        // Use blocking polling transmit from main task. This ensures:
        // 1. SPI driver handles cache coherence internally during polling
        // 2. Transaction completes before returning (no async DMA race conditions)
        // 3. No timer ISR needed for transmission
        //
        // Blocking transmit is simpler and works reliably across all ESP32 variants.

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

    // Calculate buffer size for Espressif 3-bit encoding (3 SPI bits per LED bit)
    // Each LED byte (8 bits) expands to 24 SPI bits (3 bytes)
    // Total: dataSize * 3 bytes
    const size_t spiBufferSize = dataSize * SPI_BYTES_PER_COLOR_BYTE;

    // Determine if we should use DMA
    // Use DMA for larger buffers (>64 bytes) similar to Espressif driver
    state->useDMA = (spiBufferSize > 64);

    // Espressif's led_strip driver does NOT call gpio_reset_pin() before
    // spi_bus_initialize(). The SPI driver handles GPIO configuration internally.
    FL_DBG("ChannelEngineSpi: Skipping gpio_reset_pin for GPIO " << static_cast<int>(pin) << " (following Espressif pattern)");

    // Configure SPI bus matching Espressif led_strip pattern EXACTLY
    // Reference: https://github.com/espressif/idf-extra-components/blob/master/led_strip/src/led_strip_spi_dev.c
    //
    // CRITICAL: Match Espressif led_strip_spi_dev.c configuration:
    //   .mosi_io_num = led_config->strip_gpio_num,  // Only data pin used
    //   .miso_io_num = -1,                          // Not used for LED output
    //   .sclk_io_num = -1,                          // Not used (internal timing only)
    //
    // The SPI peripheral generates clock internally (2.5MHz for 3-bit encoding) but
    // the clock signal is NEVER connected to the LED strip - only MOSI carries data.
    spi_bus_config_t bus_config = {};
    bus_config.mosi_io_num = pin;   // Data pin (only pin connected to LED strip)
    bus_config.miso_io_num = -1;    // Match Espressif led_strip: not used
    bus_config.sclk_io_num = -1;    // Match Espressif led_strip: not used (internal only)
    bus_config.quadwp_io_num = -1;  // Not used for LED strips
    bus_config.quadhd_io_num = -1;  // Not used for LED strips
    bus_config.max_transfer_sz = spiBufferSize;

    FL_DBG("ChannelEngineSpi: SPI bus config (Espressif led_strip pattern) - MOSI=" << static_cast<int>(pin)
           << ", SCLK=-1 (internal), MISO=-1 (unused)"
           << ", host=" << static_cast<int>(state->spi_host));

    // Match Espressif led_strip pattern EXACTLY: NO bus flags set
    // Reference: https://github.com/espressif/idf-extra-components/blob/master/led_strip/src/led_strip_spi_dev.c
    // Espressif led_strip does NOT set .flags at all (implicitly 0)
    // SPICOMMON_BUSFLAG_MASTER was causing issues on ESP32-S3
    // if (state->numLanes >= 4) {
    //     bus_config.flags = SPICOMMON_BUSFLAG_QUAD; // Quad mode (4 lanes)
    // } else if (state->numLanes >= 2) {
    //     bus_config.flags = SPICOMMON_BUSFLAG_DUAL; // Dual mode (2 lanes)
    // } else {
    //     bus_config.flags = 0;  // Single-lane: match Espressif (no flags)
    // }
    bus_config.flags = 0;

    // Initialize SPI bus with DMA (non-DMA has 64-byte limit)
    esp_err_t ret =
        spi_bus_initialize(state->spi_host, &bus_config, SPI_DMA_CH_AUTO);
    if (ret != ESP_OK) {
        FL_WARN("ChannelEngineSpi: spi_bus_initialize failed: " << ret);
        releaseSpiHost(state->spi_host);
        state->spi_host = SPI_HOST_MAX;
        return false;
    }

    // Configure SPI device
    // Use Espressif led_strip default: 2.5 MHz (400ns per SPI bit)
    // With 3-bit encoding: LED '0' = 100 (T0H=400ns, T0L=800ns), LED '1' = 110 (T1H=800ns, T1L=400ns)
    // Reference: https://github.com/espressif/idf-extra-components/blob/master/led_strip/src/led_strip_spi_dev.c
    constexpr uint32_t LED_STRIP_SPI_DEFAULT_RESOLUTION = 2500000;  // 2.5 MHz
    const uint32_t spi_clock_hz = LED_STRIP_SPI_DEFAULT_RESOLUTION;
    FL_DBG("ChannelEngineSpi: Using Espressif led_strip clock: " << spi_clock_hz << "Hz (2.5MHz, 3-bit encoding)");

    spi_device_interface_config_t dev_config = {};
    dev_config.clock_source = SPI_CLK_SRC_DEFAULT;  // Match Espressif led_strip (ESP-IDF 5.x)
    dev_config.command_bits = 0;
    dev_config.address_bits = 0;
    dev_config.dummy_bits = 0;
    dev_config.clock_speed_hz = spi_clock_hz;
    dev_config.mode = 0;          // SPI mode 0
    dev_config.spics_io_num = -1; // No CS pin
    dev_config.queue_size = 4;    // Transaction queue size
    // Match Espressif led_strip: NO post_cb callback
    // Reference: https://github.com/espressif/idf-extra-components/blob/master/led_strip/src/led_strip_spi_dev.c
    dev_config.post_cb = nullptr;

    FL_DBG("ChannelEngineSpi: SPI clock_hz=" << spi_clock_hz
           << " (Espressif 2.5MHz)"
           << ", bits_per_led_bit=3 (Espressif encoding)"
           << ", buffer_size=" << spiBufferSize << " bytes");

    // Device flags configuration:
    // Match Espressif led_strip pattern: minimal flags
    // Reference: https://github.com/espressif/idf-extra-components/blob/master/led_strip/src/led_strip_spi_dev.c
    // Espressif led_strip uses NO flags (dev_config.flags = 0 implicitly)
    // - SPI_DEVICE_HALFDUPLEX: Required for multi-lane (dual/quad) mode per ESP-IDF docs.
    if (state->numLanes >= 2) {
        // Multi-lane mode REQUIRES HALFDUPLEX flag per ESP-IDF documentation
        dev_config.flags = SPI_DEVICE_HALFDUPLEX;
        FL_DBG("ChannelEngineSpi: Multi-lane mode (" << static_cast<int>(state->numLanes)
               << " lanes) - using SPI_DEVICE_HALFDUPLEX");
    } else {
        // Single-lane: match Espressif led_strip (no flags)
        dev_config.flags = 0;
        FL_DBG("ChannelEngineSpi: Single-lane mode - no device flags (matches Espressif led_strip)");
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

    // Verify actual clock frequency (match Espressif led_strip tolerance: ±300 kHz)
    int actual_freq_khz = 0;
    spi_device_get_actual_freq(state->spi_device, &actual_freq_khz);
    constexpr int requested_freq_khz = LED_STRIP_SPI_DEFAULT_RESOLUTION / 1000;  // 2500 kHz
    FL_DBG("ChannelEngineSpi: Actual SPI clock frequency: " << actual_freq_khz << " kHz");
    if (actual_freq_khz < requested_freq_khz - 300 ||
        actual_freq_khz > requested_freq_khz + 300) {
        FL_WARN_ONCE("ChannelEngineSpi: Clock frequency mismatch - requested "
                << requested_freq_khz << " kHz, actual " << actual_freq_khz
                << " kHz (Espressif tolerance: ±300kHz)");
    }

    // DEBUG: Verify GPIO is configured as output by using ESP-IDF API
    // After spi_bus_initialize, the MOSI pin should be set as output
    int gpio_level = gpio_get_level(pin);
    FL_DBG("ChannelEngineSpi: GPIO " << static_cast<int>(pin) << " current level: " << gpio_level);

    // Set maximum GPIO drive capability for better signal integrity
    esp_err_t drive_err = gpio_set_drive_capability(pin, GPIO_DRIVE_CAP_3);
    if (drive_err != ESP_OK) {
        FL_WARN("ChannelEngineSpi: Failed to set GPIO drive capability: " << drive_err);
    } else {
        FL_DBG("ChannelEngineSpi: Set GPIO " << static_cast<int>(pin) << " drive to GPIO_DRIVE_CAP_3 (strongest)");
    }

    // Rely on spi_bus_initialize() for GPIO configuration (matches Espressif led_strip)
    FL_DBG("ChannelEngineSpi: Relying on spi_bus_initialize() for GPIO "
           << static_cast<int>(pin) << " routing");

    // Allocate double-buffered staging buffers (4KB each)
    // Following Espressif's led_strip driver pattern:
    // - MALLOC_CAP_INTERNAL: Forces allocation in internal SRAM
    // - MALLOC_CAP_DMA: Ensures DMA-accessible memory alignment
    // Using internal DMA memory eliminates cache coherence issues.
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

    // Zero the staging buffers - Espressif 3-bit encoding assumes zero-initialized memory
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

    // NOTE: Using Espressif 3-bit encoding (no LUT needed)
    // The encoding is done inline in preEncodeAllData() using led_strip_encode_byte()
    // This matches the Espressif led_strip reference implementation
    FL_DBG("ChannelEngineSpi: Using Espressif 3-bit encoding (no LUT)");

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
    // Following Espressif led_strip pattern: try SPI2 (FSPI) first.
    static const spi_host_device_t hosts[] = {
// #if defined(SPI2_HOST)
//         SPI2_HOST,  // FSPI - primary choice (matches Espressif led_strip)
// #endif
// #if defined(FL_IS_ESP_32S3) && defined(SPI3_HOST)
//         SPI3_HOST,  // ESP32-S3: SPI3 as fallback
// #elif defined(SPI3_HOST)
//         SPI3_HOST,  // Other variants: SPI3 as fallback
// #endif
        SPI2_HOST
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

        // Get original chipset timing for reference (passed to acquireChannel)
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
    // Pre-encode ALL LED data into staging buffer using Espressif 3-bit encoding
    // This function runs in MAIN TASK context (not ISR)
    //
    // Espressif encoding scheme:
    // - Each LED byte (8 bits) → 24 SPI bits (3 bytes)
    // - LED bit '0' → SPI bits: 100 (binary)
    // - LED bit '1' → SPI bits: 110 (binary)
    // Reference: https://github.com/espressif/idf-extra-components/blob/master/led_strip/src/led_strip_spi_dev.c
    // =========================================================================

    if (!channel || !channel->ledSource || channel->ledBytesRemaining == 0) {
        return;
    }

    size_t total_led_bytes = channel->ledBytesRemaining;
    size_t total_bytes_written = 0;

    // Reset staging to start fresh
    channel->stagingOffset = 0;
    channel->currentStaging = channel->stagingA;

    // Clear staging buffer (Espressif encoding uses |= so buffer must be zeroed)
    fl::memset(channel->currentStaging, 0, channel->stagingCapacity);

    // Encode ALL LED data into staging buffer
    // For Espressif 3-bit encoding: each LED byte expands to 3 SPI bytes
    // With 4KB staging buffer, we can hold ~1365 LED bytes (455 RGB LEDs)
    // For typical use cases, this is sufficient. For larger strips, we fill what fits.

    uint8_t* output = channel->currentStaging;
    const size_t max_output = channel->stagingCapacity;

    // NOTE: Multi-lane SPI is NOT supported with Espressif 3-bit encoding
    // The Espressif led_strip driver only supports single-lane mode
    // For multi-lane support, use PARLIO or RMT drivers instead
    if (channel->numLanes > 1) {
        FL_WARN_ONCE("ChannelEngineSpi: Multi-lane mode not supported with Espressif 3-bit encoding, using single-lane");
    }

    // Single-lane encoding - each LED byte → 3 output bytes (Espressif encoding)
    const size_t max_led_bytes = max_output / SPI_BYTES_PER_COLOR_BYTE;
    const size_t bytes_to_encode = fl_min(total_led_bytes, max_led_bytes);

    for (size_t i = 0; i < bytes_to_encode; i++) {
        uint8_t input_byte = channel->ledSource[i];
        led_strip_encode_byte(input_byte, output + total_bytes_written);
        total_bytes_written += SPI_BYTES_PER_COLOR_BYTE;
    }

    channel->ledSource += bytes_to_encode;
    channel->ledBytesRemaining -= bytes_to_encode;

    channel->stagingOffset = total_bytes_written;

    // Cache coherence: Staging buffers use MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA
    // Internal SRAM is non-cacheable for DMA, so memory barriers are sufficient.
    FL_MEMORY_BARRIER;

    FL_DBG_EVERY(100, "ChannelEngineSpi: Pre-encoded " << (total_led_bytes - channel->ledBytesRemaining)
           << " LED bytes → " << total_bytes_written << " SPI bytes");
}

void ChannelEngineSpi::transmitBlockingPolled(SpiChannelState* channel) {
    // Transmits pre-encoded LED data from staging buffer via SPI DMA.
    // Cache coherence is handled at allocation time using MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA.
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

    // Match Espressif led_strip pattern EXACTLY
    // Reference: https://github.com/espressif/idf-extra-components/blob/master/led_strip/src/led_strip_spi_dev.c
    spi_transaction_t trans;
    fl::memset(&trans, 0, sizeof(trans));  // Zero-initialize like Espressif

    trans.length = bytes_to_send * 8;  // Convert to bits
    trans.tx_buffer = channel->currentStaging;
    trans.rx_buffer = NULL;
    // NOTE: Espressif does NOT set rxlength, user, or flags - leave as 0 from memset

    // DEBUG: Log transaction configuration
    FL_DBG_EVERY(100, "ChannelEngineSpi: Transaction config - length_bits=" << trans.length
           << " (" << (trans.length / 8) << " bytes)"
           << ", tx_buffer=" << fl::ptr_to_int(trans.tx_buffer)
           << ", host=" << static_cast<int>(channel->spi_host)
           << ", device=" << fl::ptr_to_int(channel->spi_device));

    // Multi-lane mode flags (only set if needed)
    if (channel->numLanes >= 4) {
        trans.flags = SPI_TRANS_MODE_QIO;
    } else if (channel->numLanes >= 2) {
        trans.flags = SPI_TRANS_MODE_DIO;
    }
    // else: flags remain 0 from memset (single-lane, matches Espressif)

    // DEBUG: Capture first 8 bytes of tx_buffer for diagnostics
    if (bytes_to_send >= 8) {
        channel->debugTxCaptured = true;
        for (int i = 0; i < 8; i++) {
            channel->debugTxBuffer[i] = channel->currentStaging[i];
        }
    }

    // Use spi_device_transmit() (queue-based blocking API, matches Espressif led_strip)
    uint32_t start_time = millis();

    // Transmit data via SPI (queue-based API, matches Espressif led_strip)
    esp_err_t ret = spi_device_transmit(channel->spi_device, &trans);

    uint32_t elapsed_ms = millis() - start_time;

    if (ret != ESP_OK) {
        FL_WARN_EVERY(100, "ChannelEngineSpi: spi_device_transmit failed: " << ret);
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

    // No free() needed - staging buffer is managed by channel lifecycle

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
