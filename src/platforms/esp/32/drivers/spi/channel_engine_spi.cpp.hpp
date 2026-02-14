/// @file channel_engine_spi.cpp
/// @brief Clockless-over-SPI ChannelEngine implementation for ESP32
///
/// ⚠️ ARCHITECTURE NOTE: This is NOT a general SPI LED driver!
/// This driver implements CLOCKLESS protocols (WS2812, SK6812, etc.) using SPI hardware
/// as a bit-banging engine. The SPI clock is used internally for timing but is NEVER
/// physically connected to the LED strip - only the MOSI/data pin is used.
/// See channel_engine_spi.h for detailed explanation.
///
/// ENCODING: Uses wave8 encoding (8-bit expansion):
/// - Each LED bit → 8 SPI bits (1 byte per LED bit, 8 bytes per LED byte)
/// - Wave8 LUT maps nibbles to 4-byte waveform patterns
/// - SPI clock derived from chipset timing: ~6.4 MHz for WS2812
///
/// @todo Known issues and future improvements:
/// - [x] ESP32-S3 CACHE COHERENCE: Fixed by using non-aligned heap_caps_malloc()
///       which forces the SPI driver to copy data internally. This copy flushes
///       CPU cache to physical memory, solving the DMA cache coherence issue.
///       Trade-off: ~50% memory overhead for internal copy buffer.
/// - [ ] Multi-SPI host support: Currently limited by single-host-per-bus design.
/// - [ ] Multi-lane wave8: dual/quad mode via wave8 transposition

#include "platforms/is_platform.h"
#ifdef FL_IS_ESP32

#include "platforms/esp/32/feature_flags/enabled.h"

#if FASTLED_ESP32_HAS_CLOCKLESS_SPI

#include "channel_engine_spi.h"
#include "platforms/memory_barrier.h"
#include "wave8_encoder_spi.h"
#include "fl/chipsets/led_timing.h"
#include "fl/dbg.h"
#include "fl/delay.h"
#include "fl/math_macros.h"
#include "fl/warn.h"
#include "fl/stl/chrono.h"
#include "platforms/esp/32/drivers/spi/spi_hw_base.h" // SPI host definitions (SPI2_HOST, SPI3_HOST)
#include "platforms/esp/is_esp.h" // Platform detection (FL_IS_ESP_32C6, etc.)
#include "fl/compiler_control.h" // FL_MEMORY_BARRIER for cache coherence

FL_EXTERN_C_BEGIN
#include "freertos/FreeRTOS.h"  // portMAX_DELAY, pdMS_TO_TICKS
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
// Platform-specific SPI signal helpers
// ============================================================================

/// @brief Get the SPI MOSI (data output) signal index for GPIO routing
/// @param host SPI host device (SPI2_HOST, SPI3_HOST, etc.)
/// @return GPIO matrix signal index for MOSI/D output
/// @note Different ESP32 variants use different signal naming conventions:
///   - ESP32-P4: SPI2_D_PAD_OUT_IDX, SPI3_D_PAD_OUT_IDX
///   - ESP32-S2/S3/C3/C6/H2: FSPID_OUT_IDX (for SPI2_HOST/FSPI)
///   - ESP32 original: HSPID_OUT_IDX, VSPID_OUT_IDX
inline int getSpiMosiSignalIndex(spi_host_device_t host) {
#if defined(FL_IS_ESP_32P4)
    // ESP32-P4: Use SPI2_D_PAD_OUT_IDX or SPI3_D_PAD_OUT_IDX
    if (host == SPI2_HOST) {
        return SPI2_D_PAD_OUT_IDX;
    } else if (host == SPI3_HOST) {
        return SPI3_D_PAD_OUT_IDX;
    }
#elif defined(FSPID_OUT_IDX)
    // ESP32-S2/S3/C3/C6/H2: Use FSPID_OUT_IDX for SPI2_HOST (FSPI)
    if (host == SPI2_HOST) {
        return FSPID_OUT_IDX;
    }
#endif
#if defined(VSPID_OUT_IDX)
    // ESP32 original and S3: Use VSPID_OUT_IDX for SPI3_HOST (VSPI/HSPI)
    if (host == SPI3_HOST) {
        return VSPID_OUT_IDX;
    }
#endif
#if defined(HSPID_OUT_IDX)
    // ESP32 original: Use HSPID_OUT_IDX for SPI2_HOST (HSPI)
    if (host == SPI2_HOST) {
        return HSPID_OUT_IDX;
    }
#endif
    // Fallback: return -1 if no valid signal found
    FL_WARN("getSpiMosiSignalIndex: Unsupported SPI host " << host);
    return -1;
}

// ============================================================================
// Math helpers
// ============================================================================

/// @brief Greatest common divisor (Euclidean algorithm)
inline u32 gcd(u32 a, u32 b) {
    while (b != 0) {
        u32 t = b;
        b = a % b;
        a = t;
    }
    return a;
}

// ============================================================================
// Wave8 encoding constants
// ============================================================================
// Each LED bit expands to 8 SPI bits (1 byte per LED bit, 8 bytes per LED byte)
// SPI clock frequency is derived from chipset timing: 8 / (T1+T2+T3) in Hz
// For WS2812 (T1=250, T2=625, T3=375 → 1250ns period): clock = 6.4 MHz

/// @brief Number of SPI bytes per LED color byte (8:1 wave8 expansion)
constexpr size_t WAVE8_BYTES_PER_COLOR_BYTE = 8;

/// @brief Calculate SPI clock frequency for wave8 encoding from chipset timing
/// @param total_period_ns Total LED bit period (T1+T2+T3) in nanoseconds
/// @return SPI clock frequency in Hz (8 pulses per LED bit period)
constexpr u32 calculateWave8SpiClockHz(u32 total_period_ns) {
    // Wave8 expands each LED bit to 8 SPI bits
    // Pulse period = total_period_ns / 8
    // Clock frequency = 1e9 / pulse_period_ns = 8e9 / total_period_ns
    const u32 pulse_period_ns = total_period_ns / 8;
    return (pulse_period_ns > 0) ? (1000000000U / pulse_period_ns) : 6400000U;
}

} // namespace

ChannelEngineSpi::ChannelEngineSpi()
    : mAllocationFailed(false), mLastRetryFrame(0) {
    FL_DBG("ChannelEngineSpi: Constructor called");
}

ChannelEngineSpi::~ChannelEngineSpi() {
    FL_DBG("ChannelEngineSpi: Destructor called");

    // Wait for any in-flight pipeline to complete before cleanup
    while (mPipeline.mPhase != DmaPipelineState::IDLE) {
        advancePipeline();
        fl::delayMicroseconds(100);
    }

    // Poll to ensure cleanup of any in-flight transmissions
    poll();

    mMultiLaneConfigs.clear();

    // Clean up all channels
    for (auto &channel : mChannels) {
        // Free staging buffers (allocated with heap_caps_malloc)
        if (channel.stagingA) {
            heap_caps_free(channel.stagingA);
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
    //   - Clockless LED bits are encoded as SPI bit patterns via wave8 (8:1 expansion)
    //   - The SPI clock controls MOSI timing (e.g., ~6.67MHz for WS2812 = ~150ns per bit)
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

    u8 laneCount = pinConfig.getLaneCount();
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

    // Wait for any previous pipeline to complete before starting new frame
    while (mPipeline.mPhase != DmaPipelineState::IDLE) {
        advancePipeline();
        fl::delayMicroseconds(100);
    }

    // Release any channels from previous frame
    for (auto &channel : mChannels) {
        if (!channel.inUse) continue;
        if (channel.transmissionComplete) {
            if (channel.sourceData) {
                channel.sourceData->setInUse(false);
            }
            releaseChannel(&channel);
        }
    }

    // Move enqueued channels to transmitting list
    mTransmittingChannels = fl::move(mEnqueuedChannels);
    mEnqueuedChannels.clear();

    // Build timing groups for batched transmission
    mPipeline.mTimingGroups.clear();
    fl::hash_map<ChipsetTimingConfig, fl::vector<ChannelDataPtr>, TimingHash, TimingEqual> timingGroups;
    for (const auto& channel : mTransmittingChannels) {
        const ChipsetTimingConfig& timing = channel->getTiming();
        timingGroups[timing].push_back(channel);
    }

    for (const auto& [timing, groupChannels] : timingGroups) {
        DmaPipelineState::TimingGroupInfo info;
        info.timing = timing;
        info.channels = groupChannels;
        info.laneCapacity = determineLaneCapacity(groupChannels);
        mPipeline.mTimingGroups.push_back(fl::move(info));
    }

    // Initialize pipeline state
    mPipeline.mCurrentTimingGroup = 0;
    mPipeline.mCurrentBatchIdx = 0;
    mPipeline.mCurrentChannelInBatch = 0;
    mPipeline.mActiveChannel = nullptr;
    mPipeline.mDmaInFlight = false;
    mPipeline.mEncodeIdx = 0;

    // Start the first channel
    if (startNextChannel()) {
        startFirstDma();
    }
}

IChannelEngine::EngineState ChannelEngineSpi::poll() {
    // If pipeline is active, advance it
    if (mPipeline.mPhase != DmaPipelineState::IDLE) {
        return advancePipeline();
    }

    // Pipeline idle — check for any channels needing cleanup
    bool anyBusy = false;
    for (auto &channel : mChannels) {
        if (!channel.inUse) continue;
        if (channel.transmissionComplete) {
            if (channel.sourceData) {
                channel.sourceData->setInUse(false);
            }
            releaseChannel(&channel);
        } else {
            anyBusy = true;
        }
    }

    if (anyBusy)
        return EngineState::BUSY;

    // All channels complete - clear transmitting list
    if (!mTransmittingChannels.empty()) {
        mTransmittingChannels.clear();
    }

    return EngineState::READY;
}

/// @brief Poll channel states for cleanup
IChannelEngine::EngineState ChannelEngineSpi::pollChannels() {
    bool anyBusy = false;

    for (auto &channel : mChannels) {
        if (!channel.inUse)
            continue;

        if (channel.transmissionComplete) {
            if (channel.sourceData) {
                channel.sourceData->setInUse(false);
            }
            releaseChannel(&channel);
        } else {
            anyBusy = true;
        }
    }

    if (anyBusy)
        return EngineState::BUSY;

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
    //   - Group 1 (WS2812): 4 channels with ~6.67MHz clock, wave8 bit patterns
    //   - Group 2 (SK6812): 4 channels with different clock/patterns
    //
    // Hash map key: ChipsetTimingConfig (t1_ns, t2_ns, t3_ns, reset_us)
    // Hash map value: Vector of channels with matching timing
    fl::hash_map<ChipsetTimingConfig, fl::vector<ChannelDataPtr>, TimingHash, TimingEqual> timingGroups;

    for (const auto& channel : channels) {
        const ChipsetTimingConfig& timing = channel->getTiming();
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
        u8 K = determineLaneCapacity(groupChannels);
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
            EngineState state(EngineState::BUSY);
            while ((state = pollChannels()) != EngineState::READY) {
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
                       << timing.reset_us << " μs) between batches");
                fl::delayMicroseconds(timing.reset_us);
            }
        }
    }
}

u8 ChannelEngineSpi::determineLaneCapacity(
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
    constexpr u8 PARALLEL_SPI_HOSTS = 1;  // Single SPI host policy

    FL_DBG_EVERY(100, "ChannelEngineSpi: Determined lane capacity: "
           << static_cast<int>(PARALLEL_SPI_HOSTS) << " SPI hosts");
    return PARALLEL_SPI_HOSTS;
}

void ChannelEngineSpi::beginTransmission(
    fl::span<const ChannelDataPtr> channelData) {
    for (const auto &data : channelData) {
        gpio_num_t pin = static_cast<gpio_num_t>(data->getPin());

        // Get clockless timing configuration from channel data
        const ChipsetTimingConfig& timing = data->getTiming();

        // DEBUG: Trace timing values received from channel data
        FL_DBG_EVERY(100, "ChannelEngineSpi: Received timing from ChannelData: t1_ns="
               << timing.t1_ns << ", t2_ns=" << timing.t2_ns
               << ", t3_ns=" << timing.t3_ns << ", reset_us=" << timing.reset_us);

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
        }

        // Store reference to source data for cleanup
        channel->sourceData = data;

        // Double-buffered streaming transmission from main task context.
        // Overlaps DMA transfer with CPU encoding using ping-pong buffers:
        //   - Encode into buffer A, queue for DMA
        //   - Encode into buffer B while DMA sends buffer A
        //   - Wait for A to complete, queue B, encode next into A
        //   ... until all LED data is transmitted.
        //
        // Each staging buffer is 4KB, holding ~512 LED bytes (~170 RGB LEDs) with wave8.
        // Reset zeros are only appended after the LAST chunk.
        transmitStreaming(channel);

        // Mark transmission as complete (no async waiting needed)
        channel->transmissionComplete = true;
    }
}

ChannelEngineSpi::SpiChannelState *
ChannelEngineSpi::acquireChannel(gpio_num_t pin, const ChipsetTimingConfig &timing,
                                 size_t dataSize) {

    // Try to find existing idle channel with matching pin and timing
    for (auto &channel : mChannels) {
        if (!channel.inUse && channel.pin == pin && channel.timing == timing) {
            channel.inUse = true;
            channel.transmissionComplete = false;
            channel.ledBytesRemaining = 0;

            if (channel.spi_host != SPI_HOST_MAX) {
                // SPI hardware still allocated - just reroute MOSI to this pin
                // The MOSI signal might be routed to a different pin from a previous batch.
                // Use GPIO matrix to reroute SPI MOSI output to this pin.
                gpio_set_direction(pin, GPIO_MODE_OUTPUT);
                int mosiSignal = getSpiMosiSignalIndex(channel.spi_host);
                if (mosiSignal >= 0) {
                    esp_rom_gpio_connect_out_signal(pin, mosiSignal, false, false);
                    FL_DBG_EVERY(100, "ChannelEngineSpi: Rerouted MOSI to pin " << pin);
                } else {
                    FL_WARN("ChannelEngineSpi: Failed to get MOSI signal for spi_host " << channel.spi_host);
                }
                return &channel;
            }

            // SPI hardware was released - need full reinit
            // First try to reclaim SPI from another idle channel
            for (auto &other : mChannels) {
                if (!other.inUse && other.spi_host != SPI_HOST_MAX && &other != &channel) {
                    // Steal SPI device and host from the other channel
                    channel.spi_host = other.spi_host;
                    channel.spi_device = other.spi_device;
                    other.spi_host = SPI_HOST_MAX;
                    other.spi_device = nullptr;
                    // Reroute MOSI to new pin via GPIO matrix
                    gpio_set_direction(pin, GPIO_MODE_OUTPUT);
                    int mosiSignal = getSpiMosiSignalIndex(channel.spi_host);
                    if (mosiSignal >= 0) {
                        esp_rom_gpio_connect_out_signal(pin, mosiSignal, false, false);
                        FL_DBG("ChannelEngineSpi: Rerouted MOSI from pin "
                               << static_cast<int>(other.pin) << " to pin " << pin);
                    } else {
                        FL_WARN("ChannelEngineSpi: Failed to get MOSI signal for spi_host " << channel.spi_host);
                    }
                    return &channel;
                }
            }

            // No SPI hardware available anywhere - reinitialize
            FL_DBG("ChannelEngineSpi: Reinitializing SPI hardware for pin " << pin);
            if (!reinitSpiHardware(&channel, pin, dataSize)) {
                FL_WARN("ChannelEngineSpi: Failed to reinit SPI for pin " << pin);
                channel.inUse = false;
                return nullptr;
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

    // Free SPI from any idle channel on a different pin to make host available
    for (auto &other : mChannels) {
        if (!other.inUse && other.spi_host != SPI_HOST_MAX && other.pin != pin) {
            FL_DBG("ChannelEngineSpi: Freeing SPI from idle pin "
                   << static_cast<int>(other.pin) << " for new pin " << pin);
            if (other.spi_device) {
                spi_bus_remove_device(other.spi_device);
                other.spi_device = nullptr;
            }
            releaseSpiHost(other.spi_host);
            other.spi_host = SPI_HOST_MAX;
            break;
        }
    }

    // Now initialize the channel (this will attach ISR with correct pointer)
    if (!createChannel(channelPtr, pin, timing, dataSize)) {
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
        // Clear usage flags - keep SPI hardware allocated for fast reuse
        channel->inUse = false;
        channel->transmissionComplete = false;

        // Reset streaming state (prevents stale data corruption on reuse)
        channel->ledSource = nullptr;
        channel->ledBytesRemaining = 0;
        channel->stagingOffset = 0;
        channel->currentStaging = channel->stagingA; // Reset to buffer A

        // Release reference to source data
        channel->sourceData = nullptr;

        // Clear transaction flags
        channel->transAInFlight = false;
        channel->transBInFlight = false;

        // KEEP SPI hardware allocated between frames for fast reuse.
        // Teardown only happens in destructor or when the channel is actually destroyed.
        // This avoids the expensive ~1.2s SPI bus re-initialization cycle per frame
        // and ensures the GPIO stays in SPI MOSI mode (driven LOW by zero bytes at end of frame).
        FL_DBG_EVERY(100, "ChannelEngineSpi: Released channel for pin " << static_cast<int>(channel->pin)
               << " (SPI hardware kept allocated for reuse)");
    }
}

bool ChannelEngineSpi::reinitSpiHardware(SpiChannelState *state, gpio_num_t pin,
                                         size_t dataSize) {
    // Lightweight re-initialization: only SPI bus + device.
    // Staging buffers, timer ISR, and LED source buffer are already allocated.

    state->spi_host = acquireSpiHost();
    if (state->spi_host == SPI_HOST_MAX) {
        FL_WARN_EVERY(10, "ChannelEngineSpi: No available SPI host for reinit");
        return false;
    }

    const size_t staging_size = 4096;
    const size_t spiBufferSize = dataSize * WAVE8_BYTES_PER_COLOR_BYTE;
    state->useDMA = (spiBufferSize > 64);

    // Calculate SPI clock from chipset timing (wave8: 8 pulses per LED bit period)
    const u32 total_period = state->timing.total_period_ns();
    const u32 spi_clock_hz = calculateWave8SpiClockHz(total_period > 0 ? total_period : 1250);

    spi_bus_config_t bus_config = {};
    bus_config.mosi_io_num = pin;
    bus_config.miso_io_num = -1;
    bus_config.sclk_io_num = -1;
    bus_config.quadwp_io_num = -1;
    bus_config.quadhd_io_num = -1;
    bus_config.max_transfer_sz = staging_size;
    bus_config.flags = 0;

    esp_err_t ret = spi_bus_initialize(state->spi_host, &bus_config, SPI_DMA_CH_AUTO);
    if (ret != ESP_OK) {
        FL_WARN("ChannelEngineSpi: reinit spi_bus_initialize failed: " << ret);
        releaseSpiHost(state->spi_host);
        state->spi_host = SPI_HOST_MAX;
        return false;
    }

    spi_device_interface_config_t dev_config = {};
    dev_config.clock_source = SPI_CLK_SRC_DEFAULT;
    dev_config.command_bits = 0;
    dev_config.address_bits = 0;
    dev_config.dummy_bits = 0;
    dev_config.clock_speed_hz = spi_clock_hz;
    dev_config.mode = 0;
    dev_config.spics_io_num = -1;
    dev_config.queue_size = 4;
    dev_config.post_cb = nullptr;
    dev_config.flags = (state->numLanes >= 2) ? SPI_DEVICE_HALFDUPLEX : 0;

    ret = spi_bus_add_device(state->spi_host, &dev_config, &state->spi_device);
    if (ret != ESP_OK) {
        FL_WARN("ChannelEngineSpi: reinit spi_bus_add_device failed: " << ret);
        spi_bus_free(state->spi_host);
        releaseSpiHost(state->spi_host);
        state->spi_host = SPI_HOST_MAX;
        return false;
    }

    gpio_set_drive_capability(pin, GPIO_DRIVE_CAP_3);

    FL_DBG_EVERY(100, "ChannelEngineSpi: Reinit SPI hardware for pin " << pin
           << " host=" << static_cast<int>(state->spi_host));
    return true;
}

bool ChannelEngineSpi::createChannel(SpiChannelState *state, gpio_num_t pin,
                                     const ChipsetTimingConfig &timing,
                                     size_t dataSize) {
    // Safety counter to detect infinite channel creation loops
    static int creation_count = 0;
    static u32 last_reset_ms = 0;
    u32 now_ms = millis();

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

    // Staging buffer size determines max SPI transaction size (chunked transmission)
    // We use 4KB staging buffers, so max_transfer_sz matches staging capacity
    const size_t staging_size = 4096;

    // Determine if we should use DMA
    // Use DMA for larger buffers (>64 bytes)
    const size_t spiBufferSize = dataSize * WAVE8_BYTES_PER_COLOR_BYTE;
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
    // The SPI peripheral generates clock internally (wave8 clock from chipset timing) but
    // the clock signal is NEVER connected to the LED strip - only MOSI carries data.
    spi_bus_config_t bus_config = {};
    bus_config.mosi_io_num = pin;   // Data pin (only pin connected to LED strip)
    bus_config.miso_io_num = -1;    // Match Espressif led_strip: not used
    bus_config.sclk_io_num = -1;    // Match Espressif led_strip: not used (internal only)
    bus_config.quadwp_io_num = -1;  // Not used for LED strips
    bus_config.quadhd_io_num = -1;  // Not used for LED strips
    bus_config.max_transfer_sz = staging_size;  // Chunked transmission: max = staging buffer size

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
    // Calculate SPI clock from chipset timing for wave8 encoding:
    // Wave8 expands each LED bit to 8 SPI bits, so SPI clock = 8 / bit_period
    // For WS2812 (1250ns period): 8e9 / 1250 = 6.4 MHz
    const u32 total_period_ns = timing.total_period_ns();
    const u32 spi_clock_hz = calculateWave8SpiClockHz(total_period_ns > 0 ? total_period_ns : 1250);
    FL_DBG("ChannelEngineSpi: Wave8 SPI clock: " << spi_clock_hz << "Hz (8-bit expansion, period=" << total_period_ns << "ns)");

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
           << " (wave8 encoding)"
           << ", bits_per_led_bit=8 (wave8)"
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

    // Verify actual clock frequency (tolerance: ±500 kHz for wave8)
    int actual_freq_khz = 0;
    spi_device_get_actual_freq(state->spi_device, &actual_freq_khz);
    const int requested_freq_khz = static_cast<int>(spi_clock_hz / 1000);
    FL_DBG("ChannelEngineSpi: Actual SPI clock frequency: " << actual_freq_khz << " kHz (requested " << requested_freq_khz << " kHz)");
    if (actual_freq_khz < requested_freq_khz - 500 ||
        actual_freq_khz > requested_freq_khz + 500) {
        FL_WARN_ONCE("ChannelEngineSpi: Clock frequency mismatch - requested "
                << requested_freq_khz << " kHz, actual " << actual_freq_khz
                << " kHz (tolerance: ±500kHz)");
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

    // Allocate staging buffers (4KB each, double-buffered)
    // Following Espressif's led_strip driver pattern:
    // - MALLOC_CAP_INTERNAL: Forces allocation in internal SRAM
    // - MALLOC_CAP_DMA: Ensures DMA-accessible memory alignment
    // Using internal DMA memory eliminates cache coherence issues.
    //
    // Double-buffering allows overlapping DMA transfer with CPU encoding:
    //   - Encode into buffer A while DMA sends buffer B (and vice versa)
    //   - Reduces wall-clock time by overlapping encode + transfer

    state->stagingA = (u8 *)heap_caps_malloc(
        staging_size, MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA);
    state->stagingB = (u8 *)heap_caps_malloc(
        staging_size, MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA);

    if (!state->stagingA || !state->stagingB) {
        FL_WARN("ChannelEngineSpi: Failed to allocate staging buffers");
        if (state->stagingA) { heap_caps_free(state->stagingA); state->stagingA = nullptr; }
        if (state->stagingB) { heap_caps_free(state->stagingB); state->stagingB = nullptr; }
        spi_bus_remove_device(state->spi_device);
        spi_bus_free(state->spi_host);
        releaseSpiHost(state->spi_host);
        return false;
    }

    // Zero both staging buffers
    fl::memset(state->stagingA, 0, staging_size);
    fl::memset(state->stagingB, 0, staging_size);

    FL_DBG("ChannelEngineSpi: Allocated double staging buffers via heap_caps_malloc (INTERNAL|DMA)");

    state->stagingCapacity = staging_size;
    state->currentStaging = state->stagingA;
    state->stagingOffset = 0;

    // Allocate LED source buffer (internal SRAM for ISR-safe access)
    // CRITICAL: PSRAM is NOT safe for ISR access - we must copy LED data here
    // Allocate for typical LED strip size (dataSize is passed from channel data)
    state->ledSourceBuffer = (u8 *)heap_caps_malloc(
        dataSize, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    state->ledSourceBufferSize = state->ledSourceBuffer ? dataSize : 0;

    if (!state->ledSourceBuffer) {
        FL_WARN("ChannelEngineSpi: Failed to allocate LED source buffer ("
                << dataSize << " bytes) - falling back to direct access");
        // Continue without buffer - will try direct access (may fail on PSRAM)
    }

    // Initialize streaming state
    state->cacheSyncDisabled = false;  // Enable cache sync for new channel (will auto-disable if fails)
    state->ledSource = nullptr;
    state->ledBytesRemaining = 0;
    state->transAInFlight = false;
    state->transBInFlight = false;
    state->hasNewData = false;
    state->isShuttingDown = false;
    state->debugTxCaptured = false;
    fl::memset(state->debugTxBuffer, 0, sizeof(state->debugTxBuffer));

    // CRITICAL: Zero-initialize transaction structures to avoid stale data
    fl::memset(&state->transA, 0, sizeof(spi_transaction_t));
    fl::memset(&state->transB, 0, sizeof(spi_transaction_t));

    // Build wave8 LUT from chipset timing
    {
        ChipsetTiming ct;
        ct.T1 = timing.t1_ns;
        ct.T2 = timing.t2_ns;
        ct.T3 = timing.t3_ns;
        ct.RESET = timing.reset_us;
        ct.name = timing.name;
        state->wave8Lut = buildWave8ExpansionLUT(ct);
        state->wave8LutInitialized = true;
        FL_DBG("ChannelEngineSpi: Built wave8 LUT (T1=" << ct.T1 << ", T2=" << ct.T2 << ", T3=" << ct.T3 << ")");
    }

    // NOTE: Timer ISR removed. Transmission uses double-buffered streaming via
    // transmitStreaming() with spi_device_queue_trans()/spi_device_get_trans_result().

    FL_DBG_EVERY(10, "ChannelEngineSpi: Created pin=" << pin
           << " lanes=" << static_cast<int>(state->numLanes)
           << " host=" << state->spi_host);

    return true;
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

void ChannelEngineSpi::processPendingChannels() {
    // Try to process pending channels now that hardware may be available
    fl::vector_inlined<PendingChannel, 16> stillPending;

    for (auto &pending : mPendingChannels) {
        gpio_num_t pin = static_cast<gpio_num_t>(pending.pin);
        const auto &ledData = pending.data->getData();

        SpiChannelState *channel =
            acquireChannel(pin, pending.timing, ledData.size());
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

        // Double-buffered streaming transmission (same as beginTransmission)
        transmitStreaming(channel);

        // Mark transmission as complete
        channel->transmissionComplete = true;
    }

    // Replace pending list with those that couldn't be processed
    mPendingChannels = fl::move(stillPending);
}

size_t ChannelEngineSpi::encodeChunk(SpiChannelState* channel, u8* output, size_t output_capacity) {
    // Encode one chunk of LED data into the output buffer using wave8 encoding.
    // Advances channel->ledSource and decrements channel->ledBytesRemaining.
    // Appends reset zero bytes after the last chunk.
    // Returns the number of SPI bytes written.

    if (!channel || !channel->ledSource || channel->ledBytesRemaining == 0) {
        return 0;
    }

    if (!channel->wave8LutInitialized) {
        FL_WARN("ChannelEngineSpi: Wave8 LUT not initialized, cannot encode");
        return 0;
    }

    if (channel->numLanes > 1) {
        FL_WARN_ONCE("ChannelEngineSpi: Multi-lane wave8 encoding not yet implemented, using single-lane");
    }

    // Single-lane wave8 encoding via LUT
    const size_t max_led_bytes = output_capacity / WAVE8_BYTES_PER_COLOR_BYTE;
    const size_t bytes_to_encode = fl_min(channel->ledBytesRemaining, max_led_bytes);

    size_t total_bytes_written = wave8EncodeSingleLane(
        fl::span<const u8>(channel->ledSource, bytes_to_encode),
        fl::span<u8>(output, output_capacity),
        channel->wave8Lut);

    channel->ledSource += bytes_to_encode;
    channel->ledBytesRemaining -= bytes_to_encode;

    // Append reset zero bytes ONLY after the LAST chunk
    if (channel->ledBytesRemaining == 0) {
        constexpr size_t RESET_ZERO_BYTES = 50;
        const size_t available = output_capacity - total_bytes_written;
        const size_t reset_bytes = fl_min(RESET_ZERO_BYTES, available);
        if (reset_bytes > 0) {
            fl::memset(output + total_bytes_written, 0, reset_bytes);
            total_bytes_written += reset_bytes;
        }
    }

    FL_MEMORY_BARRIER;
    return total_bytes_written;
}

void ChannelEngineSpi::transmitStreaming(SpiChannelState* channel) {
    // Synchronous streaming: encode chunks and transmit via blocking SPI DMA.
    if (!channel || !channel->ledSource || channel->ledBytesRemaining == 0) {
        return;
    }

    while (channel->ledBytesRemaining > 0) {
        size_t encoded = encodeChunk(channel, channel->currentStaging, channel->stagingCapacity);
        if (encoded == 0) break;

        spi_transaction_t trans;
        fl::memset(&trans, 0, sizeof(trans));
        trans.length = encoded * 8; // Convert to bits
        trans.tx_buffer = channel->currentStaging;

        if (channel->numLanes >= 4) {
            trans.flags = SPI_TRANS_MODE_QIO;
        } else if (channel->numLanes >= 2) {
            trans.flags = SPI_TRANS_MODE_DIO;
        }

        esp_err_t ret = spi_device_transmit(channel->spi_device, &trans);
        if (ret != ESP_OK) {
            FL_WARN_EVERY(100, "ChannelEngineSpi: transmitStreaming failed: " << ret);
            break;
        }
    }

    channel->stagingOffset = 0;
}

bool ChannelEngineSpi::startNextChannel() {
    // Walk through timing groups and find the next channel to transmit
    while (mPipeline.mCurrentTimingGroup < mPipeline.mTimingGroups.size()) {
        auto& group = mPipeline.mTimingGroups[mPipeline.mCurrentTimingGroup];

        while (mPipeline.mCurrentChannelInBatch < group.channels.size()) {
            auto& data = group.channels[mPipeline.mCurrentChannelInBatch];
            mPipeline.mCurrentChannelInBatch++;

            gpio_num_t pin = static_cast<gpio_num_t>(data->getPin());
            const auto& timing = data->getTiming();
            const auto& ledData = data->getData();
            if (ledData.empty()) continue;

            SpiChannelState* ch = acquireChannel(pin, timing, ledData.size());
            if (!ch) continue;

            data->setInUse(true);
            ch->stagingOffset = 0;
            ch->currentStaging = ch->stagingA;
            ch->transAInFlight = false;
            ch->transBInFlight = false;
            ch->transmissionComplete = false;

            // Copy LED data to internal SRAM buffer for DMA-safe access
            if (ch->ledSourceBuffer && ch->ledSourceBufferSize >= ledData.size()) {
                fl::memcpy(ch->ledSourceBuffer, ledData.data(), ledData.size());
                ch->ledSource = ch->ledSourceBuffer;
            } else {
                ch->ledSource = ledData.data();
            }
            ch->ledBytesRemaining = ledData.size();
            ch->sourceData = data;

            mPipeline.mActiveChannel = ch;
            mPipeline.mPhase = DmaPipelineState::STREAMING;
            mPipeline.mEncodeIdx = 0;
            mPipeline.mDmaInFlight = false;
            return true;
        }

        mPipeline.mCurrentTimingGroup++;
        mPipeline.mCurrentChannelInBatch = 0;
    }

    return false;
}

void ChannelEngineSpi::startFirstDma() {
    auto* ch = mPipeline.mActiveChannel;
    if (!ch || !ch->ledSource || ch->ledBytesRemaining == 0) {
        mPipeline.mPhase = DmaPipelineState::IDLE;
        return;
    }

    // Encode first chunk into staging buffer A
    size_t encoded = encodeChunk(ch, ch->stagingA, ch->stagingCapacity);
    if (encoded == 0) {
        ch->transmissionComplete = true;
        mPipeline.mPhase = DmaPipelineState::IDLE;
        return;
    }

    // Setup and queue first DMA transaction
    spi_transaction_t* trans = &ch->transA;
    fl::memset(trans, 0, sizeof(*trans));
    trans->length = encoded * 8; // Convert to bits
    trans->tx_buffer = ch->stagingA;

    if (ch->numLanes >= 4) {
        trans->flags = SPI_TRANS_MODE_QIO;
    } else if (ch->numLanes >= 2) {
        trans->flags = SPI_TRANS_MODE_DIO;
    }

    esp_err_t ret = spi_device_queue_trans(ch->spi_device, trans, portMAX_DELAY);
    if (ret == ESP_OK) {
        ch->transAInFlight = true;
        mPipeline.mDmaInFlight = true;
        mPipeline.mEncodeIdx = 1; // Next encode goes to buffer B
        mPipeline.mPhase = DmaPipelineState::STREAMING;
    } else {
        FL_WARN("ChannelEngineSpi: startFirstDma queue_trans failed: " << ret);
        ch->transmissionComplete = true;
        mPipeline.mPhase = DmaPipelineState::IDLE;
    }
}

IChannelEngine::EngineState ChannelEngineSpi::advancePipeline() {
    auto* ch = mPipeline.mActiveChannel;

    switch (mPipeline.mPhase) {
    case DmaPipelineState::IDLE:
        return EngineState::READY;

    case DmaPipelineState::STREAMING: {
        if (!ch) {
            mPipeline.mPhase = DmaPipelineState::IDLE;
            return EngineState::READY;
        }

        // Check if current DMA is complete
        if (mPipeline.mDmaInFlight) {
            spi_transaction_t* completed;
            esp_err_t ret = spi_device_get_trans_result(ch->spi_device, &completed, 0);
            if (ret != ESP_OK) {
                return EngineState::BUSY; // DMA still in flight
            }
            if (completed == &ch->transA) ch->transAInFlight = false;
            else if (completed == &ch->transB) ch->transBInFlight = false;
            mPipeline.mDmaInFlight = false;
        }

        // Encode and queue next chunk if data remains
        if (ch->ledBytesRemaining > 0) {
            int bufIdx = mPipeline.mEncodeIdx & 1;
            u8* buffer = (bufIdx == 0) ? ch->stagingA : ch->stagingB;
            spi_transaction_t* trans = (bufIdx == 0) ? &ch->transA : &ch->transB;

            size_t encoded = encodeChunk(ch, buffer, ch->stagingCapacity);
            if (encoded > 0) {
                fl::memset(trans, 0, sizeof(*trans));
                trans->length = encoded * 8;
                trans->tx_buffer = buffer;

                if (ch->numLanes >= 4) trans->flags = SPI_TRANS_MODE_QIO;
                else if (ch->numLanes >= 2) trans->flags = SPI_TRANS_MODE_DIO;

                esp_err_t ret = spi_device_queue_trans(ch->spi_device, trans, portMAX_DELAY);
                if (ret == ESP_OK) {
                    if (bufIdx == 0) ch->transAInFlight = true;
                    else ch->transBInFlight = true;
                    mPipeline.mDmaInFlight = true;
                    mPipeline.mEncodeIdx++;
                }
            }
            return EngineState::BUSY;
        }

        // No more data to encode
        if (mPipeline.mDmaInFlight) {
            mPipeline.mPhase = DmaPipelineState::COMPLETING;
            return EngineState::BUSY;
        }

        // All done for this channel
        ch->transmissionComplete = true;
        ch->stagingOffset = 0;
        releaseChannel(ch);

        if (startNextChannel()) {
            startFirstDma();
            return EngineState::BUSY;
        }
        mPipeline.mPhase = DmaPipelineState::IDLE;
        mPipeline.mActiveChannel = nullptr;
        return EngineState::READY;
    }

    case DmaPipelineState::COMPLETING: {
        if (!ch) {
            mPipeline.mPhase = DmaPipelineState::IDLE;
            return EngineState::READY;
        }

        // Wait for final DMA to complete
        if (ch->transAInFlight || ch->transBInFlight) {
            spi_transaction_t* completed;
            esp_err_t ret = spi_device_get_trans_result(ch->spi_device, &completed, 0);
            if (ret != ESP_OK) return EngineState::BUSY;
            if (completed == &ch->transA) ch->transAInFlight = false;
            else if (completed == &ch->transB) ch->transBInFlight = false;
            if (ch->transAInFlight || ch->transBInFlight) return EngineState::BUSY;
        }

        ch->transmissionComplete = true;
        ch->stagingOffset = 0;
        releaseChannel(ch);

        if (startNextChannel()) {
            startFirstDma();
            return EngineState::BUSY;
        }
        mPipeline.mPhase = DmaPipelineState::IDLE;
        mPipeline.mActiveChannel = nullptr;
        return EngineState::READY;
    }

    case DmaPipelineState::RESET_DELAY: {
        u32 now = fl::micros();
        if ((now - mPipeline.mResetDelayStartUs) >= mPipeline.mResetDelayDurationUs) {
            if (startNextChannel()) {
                startFirstDma();
                return EngineState::BUSY;
            }
            mPipeline.mPhase = DmaPipelineState::IDLE;
            mPipeline.mActiveChannel = nullptr;
            return EngineState::READY;
        }
        return EngineState::BUSY;
    }
    }

    return EngineState::READY;
}

} // namespace fl

#endif // FASTLED_ESP32_HAS_CLOCKLESS_SPI

#endif // FL_IS_ESP32
