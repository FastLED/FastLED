// IWYU pragma: private

/// @file channel_driver_spi.cpp
/// @brief Clockless-over-SPI ChannelEngine implementation for ESP32
///
/// WARNING: ARCHITECTURE NOTE: This is NOT a general SPI LED driver!
/// This driver implements CLOCKLESS protocols (WS2812, SK6812, etc.) using SPI hardware
/// as a bit-banging driver. The SPI clock is used internally for timing but is NEVER
/// physically connected to the LED strip - only the MOSI/data pin is used.
/// See channel_driver_spi.h for detailed explanation.
///
/// ENCODING: Uses wave8 encoding (8-bit expansion):
/// - Each LED bit -> 8 SPI bits (1 byte per LED bit, 8 bytes per LED byte)
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
// Allow compilation on both ESP32 hardware and stub platforms (for testing)
#if defined(FL_IS_ESP32) || defined(FASTLED_STUB_IMPL)

#ifdef FL_IS_ESP32
#include "platforms/esp/32/feature_flags/enabled.h"
#endif

// On ESP32: Check FASTLED_ESP32_HAS_CLOCKLESS_SPI
// On stub platforms: Always allow compilation (host tests use the mock peripheral)
#if !defined(FL_IS_ESP32) || FASTLED_ESP32_HAS_CLOCKLESS_SPI

#include "platforms/esp/32/drivers/spi/channel_driver_spi.h"
#include "platforms/memory_barrier.h"
#include "platforms/esp/32/drivers/spi/wave8_encoder_spi.h"
#include "fl/chipsets/led_timing.h"
#include "fl/log/log.h"
#include "fl/system/delay.h"
#include "fl/system/pin.h"
#include "fl/math/math.h"
#include "fl/task/executor.h"
#include "fl/stl/chrono.h"
#include "platforms/esp/is_esp.h" // Platform detection (FL_IS_ESP_32C6, etc.)
#include "fl/stl/noexcept.h"
#include "fl/stl/flat_map.h"

// All ESP-IDF calls go through ISpiPeripheral. The #ifdef is isolated to
// choosing the concrete peripheral (real hardware or mock).
#ifndef FASTLED_STUB_IMPL
#include "platforms/esp/32/drivers/spi/spi_peripheral_esp.h"
#else
#include "platforms/shared/mock/esp/32/drivers/spi_peripheral_mock.h"
#endif

namespace fl {

// Static member initialization
ChannelEngineSpi::SpiHostTracking ChannelEngineSpi::sSpiHostUsage = {};

namespace {

/// @brief Platform default peripheral (real hardware or mock singleton)
detail::ISpiPeripheral& defaultSpiPeripheral() FL_NO_EXCEPT {
#ifdef FASTLED_STUB_IMPL
    return detail::SpiPeripheralMock::instance();
#else
    return detail::SpiPeripheralESP::instance();
#endif
}

// ============================================================================
// Math helpers
// ============================================================================

/// @brief Greatest common divisor (Euclidean algorithm)
inline u32 gcd(u32 a, u32 b) FL_NO_EXCEPT {
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
// For WS2812 (T1=250, T2=625, T3=375 -> 1250ns period): clock = 6.4 MHz

/// @brief Number of SPI bytes per LED color byte (8:1 wave8 expansion)
constexpr size_t WAVE8_BYTES_PER_COLOR_BYTE = 8;

/// @brief Calculate SPI clock frequency for wave8 encoding from chipset timing
/// @param total_period_ns Total LED bit period (T1+T2+T3) in nanoseconds
/// @return SPI clock frequency in Hz (8 pulses per LED bit period)
constexpr u32 calculateWave8SpiClockHz(u32 total_period_ns) FL_NO_EXCEPT {
    // Wave8 expands each LED bit to 8 SPI bits
    // Pulse period = total_period_ns / 8
    // Clock frequency = 1e9 / pulse_period_ns = 8e9 / total_period_ns
    return (total_period_ns / 8 > 0) ? (1000000000U / (total_period_ns / 8)) : 6400000U;
}

/// @brief SPI transfer mode: split polling (ESP32-C6) vs async interrupt-driven (other variants)
///
/// ESP32-C6's async DMA path (spi_device_queue_trans) does not produce GPIO output on the
/// MOSI pin, even though transactions complete successfully. Root cause: GDMA-to-SPI
/// peripheral timing sensitivity on the C6's RISC-V single-core architecture - the ISR-driven
/// transaction start introduces variable timing that violates the GDMA outlink-to-SPI-USR
/// register write timing requirement (see esp-rs/esp-hal#489 where 2 CPU cycles determined
/// success vs failure). Split polling mode (spi_device_polling_start + spi_device_polling_end)
/// uses deterministic CPU-driven register writes, avoiding this race condition, while still
/// allowing non-blocking completion checks for true encode/transmit overlap.
#ifdef FL_IS_ESP_32C6
constexpr bool kSpiUsePolling = true;
#else
constexpr bool kSpiUsePolling = false;
#endif

/// @brief Start an SPI transaction (non-blocking on both paths)
/// @return true on success
/// On ESP32-C6: uses spi_device_polling_start (split polling, non-blocking start)
/// On other variants: uses spi_device_queue_trans (ISR-driven async DMA)
inline bool spiStart(detail::ISpiPeripheral& peripheral, const detail::SpiTransaction& trans) FL_NO_EXCEPT {
    if (kSpiUsePolling) {
        return peripheral.pollingStart(trans);
    }
    return peripheral.queueTransaction(trans);
}

/// @brief Check if in-flight transaction is done (non-blocking)
/// @param out_user [out] For async mode, receives the completed transaction's user field
/// @return true if done, false if still in flight
/// On ESP32-C6: uses spi_device_polling_end with 0 tick timeout
/// On other variants: uses spi_device_get_trans_result with 0 tick timeout
inline bool spiCheckDone(detail::ISpiPeripheral& peripheral, void** out_user) FL_NO_EXCEPT {
    if (kSpiUsePolling) {
        return peripheral.pollingEnd(0);
    }
    return peripheral.pollTransaction(0, out_user);
}

/// @brief Build a transaction whose user field identifies @p slot
inline void setupTransaction(detail::SpiTransaction* slot, const u8* buffer,
                             size_t bytes, u8 numLanes) FL_NO_EXCEPT {
    *slot = detail::SpiTransaction(buffer, bytes, slot);
    if (numLanes >= 4) {
        slot->flags = detail::kSpiTransModeQio;
    } else if (numLanes >= 2) {
        slot->flags = detail::kSpiTransModeDio;
    }
}

} // namespace

ChannelEngineSpi::ChannelEngineSpi() FL_NO_EXCEPT
    : ChannelEngineSpi(defaultSpiPeripheral()) {}

ChannelEngineSpi::ChannelEngineSpi(detail::ISpiPeripheral& peripheral) FL_NO_EXCEPT
    : mPeripheral(peripheral), mAllocationFailed(false), mLastRetryFrame(0) {
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
        // Free staging buffers (allocated with allocateDma)
        if (channel.stagingA) {
            mPeripheral.freeDma(channel.stagingA);
        }
        if (channel.stagingB) {
            mPeripheral.freeDma(channel.stagingB);
        }
        // Free LED source buffer
        if (channel.ledSourceBuffer) {
            mPeripheral.freeInternal(channel.ledSourceBuffer);
        }

        // Remove SPI device
        if (channel.hasDevice) {
            // Wait for any pending transactions
            if (channel.transAInFlight || channel.transBInFlight) {
                mPeripheral.pollTransaction(1000, nullptr);
                if (channel.transAInFlight || channel.transBInFlight) {
                    mPeripheral.pollTransaction(1000, nullptr);
                }
            }
            mPeripheral.removeDevice();
        }

        // Release SPI host
        if (channel.spi_host != kNoSpiHost) {
            releaseSpiHost(channel.spi_host);
        }

        // Restore GPIO pulldown when SPI channel is destroyed
        // This ensures pin returns to stable LOW state
        (fl::pinMode)(channel.pin, fl::PinMode::InputPulldown);
    }
}

bool ChannelEngineSpi::canHandle(const ChannelDataPtr& data) const FL_NO_EXCEPT {
    if (!data) {
        return false;
    }

    // WARNING: ARCHITECTURE CLARIFICATION: This is a CLOCKLESS-over-SPI driver!
    //
    // This driver uses SPI hardware to implement CLOCKLESS LED protocols (WS2812, SK6812, etc.),
    // NOT true SPI protocols (APA102, SK9822, etc.). The SPI clock pin is used internally for
    // precise timing generation but is NEVER physically connected to the LED strip - only the
    // MOSI/data pin carries signals to the LEDs.
    //
    // How it works:
    //   - Clockless LED bits are encoded as SPI bit patterns via wave8 (8:1 expansion)
    //   - The SPI clock controls MOSI timing (e.g., ~6.67MHz for WS2812 = ~150ns per bit)
    //   - LEDs decode pulse widths on the data line, ignoring the clock signal
    //
    //  CORRECTED LOGIC:
    // Accept CLOCKLESS chipsets (WS2812, SK6812), reject TRUE SPI chipsets (APA102, SK9822)
    // True SPI chipsets should route to SpiChannelEngineAdapter (priority 5-9), not this driver
    //
    // Correct routing:
    //   APA102 -> SpiChannelEngineAdapter (true SPI hardware)
    //   WS2812 -> ChannelEngineSpi (this driver, clockless-over-SPI)
    return !data->isSpi();  //  FIXED: Accept clockless, reject true SPI
}

void ChannelEngineSpi::configureMultiLanePins(
    const MultiLanePinConfig &pinConfig) FL_NO_EXCEPT {
    if (pinConfig.data0_pin < 0) {
        FL_WARN("ChannelEngineSpi: Invalid multi-lane config - data0_pin must "
                "be >= 0");
        return;
    }

    u8 laneCount = pinConfig.getLaneCount();
    FL_DBG("ChannelEngineSpi: Configuring " << (static_cast<int>(laneCount)) << "-lane SPI for pin " << pinConfig.data0_pin << " (data0=" << pinConfig.data0_pin << ", data1=" << pinConfig.data1_pin << ", data2=" << pinConfig.data2_pin << ", data3=" << pinConfig.data3_pin << ")");

// Validate platform capabilities
#if defined(FL_IS_ESP_32C6) || defined(FL_IS_ESP_32C3) ||                      \
    defined(FL_IS_ESP_32H2)
    // ESP32-C6/C3/H2: Dual-lane max (no quad support)
    if (laneCount > 2) {
        FL_WARN("ChannelEngineSpi: ESP32-C6/C3/H2 only supports dual-lane SPI (max 2 lanes), requested " << (static_cast<int>(laneCount)) << " lanes");
        return;
    }
#endif

    // Store the configuration
    mMultiLaneConfigs[pinConfig.data0_pin] = pinConfig;
    FL_DBG("ChannelEngineSpi: Multi-lane configuration stored for pin " << pinConfig.data0_pin);
}

void ChannelEngineSpi::enqueue(ChannelDataPtr channelData) FL_NO_EXCEPT {
    if (!channelData) {
        FL_WARN("ChannelEngineSpi: Null channel data passed to enqueue()");
        return;
    }

    mEnqueuedChannels.push_back(channelData);
}

void ChannelEngineSpi::show() FL_NO_EXCEPT {
    if (mEnqueuedChannels.empty()) {
        return;
    }

    if (!waitForReady()) {
        // Drop the new batch while DMA still owns the active pipeline.
        mEnqueuedChannels.clear();
        return;
    }

    // Release any channels from previous frame
    for (auto &channel : mChannels) {
        if (!channel.inUse) continue;
        if (channel.transmissionComplete) {
            releaseChannel(&channel);
        }
    }

    // Move enqueued channels to transmitting list
    mTransmittingChannels = fl::move(mEnqueuedChannels);
    mEnqueuedChannels.clear();

    // Build timing groups for batched transmission
    mPipeline.mTimingGroups.clear();
    fl::flat_map<ChipsetTimingConfig, fl::vector<ChannelDataPtr>, TimingLess> timingGroups;
    for (const auto& channel : mTransmittingChannels) {
        const ChipsetTimingConfig& timing = channel->getTiming();
        timingGroups[timing].push_back(channel);
    }

    for (const auto& group : timingGroups) {
        const ChipsetTimingConfig& timing = group.first;
        const fl::vector<ChannelDataPtr>& groupChannels = group.second;
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

    // Start the first channel's DMA pipeline.
    // poll() advances the pipeline by checking completion and encoding/queueing
    // subsequent chunks. On ESP32-C6, each step uses polling SPI (synchronous
    // per-chunk); on other variants, async interrupt-driven DMA.
    if (startNextChannel()) {
        startFirstDma();
    }
}

IChannelDriver::DriverState ChannelEngineSpi::poll() FL_NO_EXCEPT {
    // If pipeline is active, advance it
    if (mPipeline.mPhase != DmaPipelineState::IDLE) {
        return advancePipeline();
    }

    // Pipeline idle - check for any channels needing cleanup
    bool anyDraining = false;
    for (auto &channel : mChannels) {
        if (!channel.inUse) continue;
        if (channel.transmissionComplete) {
            releaseChannel(&channel);
        } else {
            anyDraining = true;
        }
    }

    if (anyDraining)
        return DriverState::DRAINING;

    // All channels complete - clear transmitting list
    if (!mTransmittingChannels.empty()) {
        mTransmittingChannels.clear();
    }

    return DriverState::READY;
}

/// @brief Poll channel states for cleanup
IChannelDriver::DriverState ChannelEngineSpi::pollChannels() FL_NO_EXCEPT {
    bool anyDraining = false;

    for (auto &channel : mChannels) {
        if (!channel.inUse)
            continue;

        if (channel.transmissionComplete) {
            releaseChannel(&channel);
        } else {
            anyDraining = true;
        }
    }

    if (anyDraining)
        return DriverState::DRAINING;

    return DriverState::READY;
}

void ChannelEngineSpi::beginBatchedTransmission(
    fl::span<const ChannelDataPtr> channels) FL_NO_EXCEPT {

    // Safety check: Pending queue should be empty before starting new batches
    // If pending channels exist, it indicates incomplete transmission from previous frame
    // or hardware saturation, which would interfere with batching logic.
    if (!mPendingChannels.empty()) {
        FL_WARN_EVERY(100, "ChannelEngineSpi: Pending queue not empty at batch start (" << mPendingChannels.size() << " channels pending). This may indicate hardware saturation or incomplete previous frame.");
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
    fl::flat_map<ChipsetTimingConfig, fl::vector<ChannelDataPtr>, TimingLess> timingGroups;

    for (const auto& channel : channels) {
        const ChipsetTimingConfig& timing = channel->getTiming();
        timingGroups[timing].push_back(channel);
    }

    FL_DBG_EVERY(100, "ChannelEngineSpi: Grouped " << channels.size() << " channels into " << timingGroups.size() << " timing groups");

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
    for (const auto& group : timingGroups) {
        const ChipsetTimingConfig& timing = group.first;
        const fl::vector<ChannelDataPtr>& groupChannels = group.second;
        // Determine lane capacity (K) for this timing group
        // K = max number of channels that can transmit simultaneously
        u8 K = determineLaneCapacity(groupChannels);
        size_t N = groupChannels.size();
        size_t numBatches = (N + K - 1) / K;  // ceil(N/K)

        FL_DBG_EVERY(100, "ChannelEngineSpi: Timing group with " << N << " channels, " << (static_cast<int>(K)) << " lanes -> " << numBatches << " batches");

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
        //   READY -> beginTransmission() -> BUSY -> DRAINING -> READY
        for (size_t batchIdx = 0; batchIdx < numBatches; batchIdx++) {
            size_t batchStart = batchIdx * K;
            size_t batchEnd = min(batchStart + K, N);


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
            DriverState state{DriverState::BUSY};
            while ((state = pollChannels()) != DriverState::READY) {
                if (state == DriverState::ERROR) {
                    FL_WARN_EVERY(10, "ChannelEngineSpi: Error during batch transmission");
                    break;
                }
                // OS yield only while waiting for DMA
                task::yield_system(250);
            }

            // Insert reset delay between batches (critical for LED protocol compliance)
            // LEDs require a reset pulse (low signal) to latch data and prepare for next frame.
            // Without this delay, LEDs interpret the next batch as frame continuation,
            // causing alternating black/color frames (protocol violation).
            if (batchIdx + 1 < numBatches) {
                FL_DBG_EVERY(100, "ChannelEngineSpi: Inserting reset delay (" << timing.reset_us << " us) between batches");
                fl::delayMicroseconds(timing.reset_us);
            }
        }
    }
}

u8 ChannelEngineSpi::determineLaneCapacity(
    fl::span<const ChannelDataPtr> channels) FL_NO_EXCEPT {

    // Determine the maximum number of channels that can transmit in parallel (K).
    //
    // ESP32 SPI Hardware Limitation:
    //   - Each SPI host (SPI2_HOST, SPI3_HOST) can only have ONE bus configuration
    //   - Each host uses one set of pins, so only ONE channel per host
    //   - See acquireSpiHost() at line 756: "if (tracking->refCount == 0)"
    //
    // Platform Capacity:
    //   - ESP32/S2/S3/P4: 2 SPI hosts -> K=2 (parallel transmission)
    //   - ESP32-C3: 1 SPI host -> K=1 (sequential transmission)
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

    FL_DBG_EVERY(100, "ChannelEngineSpi: Determined lane capacity: " << (static_cast<int>(PARALLEL_SPI_HOSTS)) << " SPI hosts");
    return PARALLEL_SPI_HOSTS;
}

void ChannelEngineSpi::beginTransmission(
    fl::span<const ChannelDataPtr> channelData) FL_NO_EXCEPT {
    for (const auto &data : channelData) {
        int pin = data->getPin();

        // Get clockless timing configuration from channel data
        const ChipsetTimingConfig& timing = data->getTiming();

        // DEBUG: Trace timing values received from channel data
        FL_DBG_EVERY(100, "ChannelEngineSpi: Received timing from ChannelData: t1_ns=" << timing.t1_ns << ", t2_ns=" << timing.t2_ns << ", t3_ns=" << timing.t3_ns << ", reset_us=" << timing.reset_us);

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
            FL_DBG("ChannelEngineSpi: No HW available for pin " << pin << ", queuing");
            PendingChannel pending;
            pending.data = data;
            pending.pin = pin;
            pending.timing = timing;
            mPendingChannels.push_back(pending);
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
            FL_DBG_EVERY(100, "ChannelEngineSpi: Input LED data (first 6 bytes): [" << (static_cast<int>(ledData[0])) << "," << (static_cast<int>(ledData[1])) << "," << (static_cast<int>(ledData[2])) << "," << (static_cast<int>(ledData[3])) << "," << (static_cast<int>(ledData[4])) << "," << (static_cast<int>(ledData[5])) << "]");
        }

        // Store reference to source data for cleanup
        channel->sourceData = data;

        // Double-buffered streaming transmission from main task context.
        transmitStreaming(channel);

        // Mark transmission as complete (no async waiting needed)
        channel->transmissionComplete = true;
    }
}

ChannelEngineSpi::SpiChannelState *
ChannelEngineSpi::acquireChannel(int pin, const ChipsetTimingConfig &timing,
                                 size_t dataSize) FL_NO_EXCEPT {

    // Try to find existing idle channel with matching pin and timing
    for (auto &channel : mChannels) {
        if (!channel.inUse && channel.pin == pin && channel.timing == timing) {
            channel.inUse = true;
            channel.transmissionComplete = false;
            channel.ledBytesRemaining = 0;

            // FastLED#4907: while this channel sat idle, another driver (or
            // pinMode()) could take its pin, and the bus would keep shifting
            // into a pin that no longer carries MOSI. Rerouting MOSI alone is
            // not enough on ESP32-C6 (see below), so release the host and let
            // the reinit path rebuild the bus on this pin.
            if (channel.spi_host != kNoSpiHost &&
                channel.pinRouting != detail::ISpiPeripheral::kPinRoutingUnknown &&
                mPeripheral.getPinRouting(pin) != channel.pinRouting) {
                FL_DBG("ChannelEngineSpi: GPIO " << pin << " was rerouted while idle - rebuilding its SPI bus");
                if (channel.hasDevice) {
                    mPeripheral.removeDevice();
                    channel.hasDevice = false;
                }
                releaseSpiHost(channel.spi_host);
                channel.spi_host = kNoSpiHost;
            }

            if (channel.spi_host != kNoSpiHost) {
                // SPI hardware still allocated - reuse directly (same pin + timing).
                // Reallocate LED source buffer if the new data is larger
                // than the existing buffer. Without this, larger strips
                // fall back to ISR-unsafe direct PSRAM access.
                if (channel.ledSourceBufferSize < dataSize) {
                    if (channel.ledSourceBuffer) {
                        mPeripheral.freeInternal(channel.ledSourceBuffer);
                    }
                    channel.ledSourceBuffer = mPeripheral.allocateInternal(dataSize);
                    channel.ledSourceBufferSize =
                        channel.ledSourceBuffer ? dataSize : 0;
                    if (!channel.ledSourceBuffer) {
                        FL_WARN("ChannelEngineSpi: Failed to reallocate LED source buffer (" << dataSize << " bytes)");
                    }
                }
                FL_DBG_EVERY(100, "ChannelEngineSpi: Reusing SPI for pin " << pin);
                return &channel;
            }

            // SPI hardware was released - need full reinit
            // First try to free SPI from another idle channel to make host available
            for (auto &other : mChannels) {
                if (!other.inUse && other.spi_host != kNoSpiHost && &other != &channel) {
                    // On ESP32-C6 (and potentially other variants), esp_rom_gpio_connect_out_signal()
                    // is NOT sufficient to reroute SPI MOSI. Must free and reinit the bus with
                    // the correct mosi_io_num via spi_bus_initialize().
                    FL_DBG("ChannelEngineSpi: Freeing SPI from idle pin " << (static_cast<int>(other.pin)) << " for reuse by pin " << pin);
                    if (other.hasDevice) {
                        mPeripheral.removeDevice();
                        other.hasDevice = false;
                    }
                    releaseSpiHost(other.spi_host);
                    other.spi_host = kNoSpiHost;
                    break;
                }
            }

            // Reinitialize SPI hardware with the correct pin
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
    newChannel.spi_host = kNoSpiHost;
    newChannel.hasDevice = false;

    // Check if there's a multi-lane configuration for this pin
    auto it = mMultiLaneConfigs.find(pin);
    if (it != mMultiLaneConfigs.end()) {
        // Apply multi-lane configuration
        const MultiLanePinConfig &config = it->second;
        newChannel.numLanes = config.getLaneCount();
        newChannel.data1_pin = config.data1_pin;
        newChannel.data2_pin = config.data2_pin;
        newChannel.data3_pin = config.data3_pin;
        FL_DBG("ChannelEngineSpi: Applying " << (static_cast<int>(newChannel.numLanes)) << "-lane configuration for pin " << pin);
    } else {
        // Default to single-lane mode
        newChannel.numLanes = 1;
        newChannel.data1_pin = -1;
        newChannel.data2_pin = -1;
        newChannel.data3_pin = -1;
    }

    // Add to channel list BEFORE calling createChannel() to ensure stable
    // pointer
    mChannels.push_back(newChannel);
    SpiChannelState *channelPtr = &mChannels.back();

    // Free SPI from any idle channel on a different pin to make host available
    for (auto &other : mChannels) {
        if (!other.inUse && other.spi_host != kNoSpiHost && other.pin != pin) {
            FL_DBG("ChannelEngineSpi: Freeing SPI from idle pin " << (static_cast<int>(other.pin)) << " for new pin " << pin);
            if (other.hasDevice) {
                mPeripheral.removeDevice();
                other.hasDevice = false;
            }
            releaseSpiHost(other.spi_host);
            other.spi_host = kNoSpiHost;
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

    FL_DBG("ChannelEngineSpi: Created new channel for pin " << pin << " (total: " << mChannels.size() << ")");
    return channelPtr;
}

void ChannelEngineSpi::releaseChannel(SpiChannelState *channel) FL_NO_EXCEPT {
    if (channel) {
        // Clear usage flags - keep SPI hardware allocated for fast reuse
        channel->inUse = false;
        channel->transmissionComplete = false;

        // Reset streaming state (prevents stale data corruption on reuse)
        channel->ledSource = nullptr;
        channel->ledBytesRemaining = 0;
        channel->stagingOffset = 0;
        channel->currentStaging = channel->stagingA; // Reset to buffer A

        if (channel->sourceData) {
            channel->sourceData->setInUse(false);
            channel->sourceData = nullptr;
        }

        // Clear transaction flags
        channel->transAInFlight = false;
        channel->transBInFlight = false;

        // KEEP SPI hardware allocated between frames for fast reuse.
        // Teardown only happens in destructor or when the channel is actually destroyed.
        // This avoids the expensive ~1.2s SPI bus re-initialization cycle per frame
        // and ensures the GPIO stays in SPI MOSI mode (driven LOW by zero bytes at end of frame).
        FL_DBG_EVERY(100, "ChannelEngineSpi: Released channel for pin " << (static_cast<int>(channel->pin)) << " (SPI hardware kept allocated for reuse)");
    }
}

bool ChannelEngineSpi::reinitSpiHardware(SpiChannelState *state, int pin,
                                         size_t dataSize) FL_NO_EXCEPT {
    // Lightweight re-initialization: only SPI bus + device.
    // Staging buffers, timer ISR, and LED source buffer are already allocated.

    state->spi_host = acquireSpiHost();
    if (state->spi_host == kNoSpiHost) {
        FL_WARN_EVERY(10, "ChannelEngineSpi: No available SPI host for reinit");
        return false;
    }

    // Staging buffer sized to fit an entire ~680-LED WS2812B wave8 payload in
    // one SPI transaction (1 LED = 24 wave8 bytes). With 4096-byte staging,
    // any strip longer than ~170 LEDs was transmitted as multiple chunks with
    // a poll()-timing-dependent gap between them - long enough to latch the
    // first 170 LEDs and leave the rest dark (the "black frame between
    // displays" bug in #2254). 16384 bytes lets ESP-IDF's SPI DMA descriptor
    // chain produce continuous output for a single transaction.
    const size_t staging_size = 16384;
    const size_t spiBufferSize = dataSize * WAVE8_BYTES_PER_COLOR_BYTE;
    state->useDMA = (spiBufferSize > 64);

    // Calculate SPI clock from chipset timing (wave8: 8 pulses per LED bit period)
    const u32 total_period = state->timing.total_period_ns();
    const u32 spi_clock_hz = calculateWave8SpiClockHz(total_period > 0 ? total_period : 1250);

    // Reset GPIO before SPI reinit (required for RTC GPIOs on ESP32-C6)
    mPeripheral.resetPin(pin);

    // MOSI only; SCLK, MISO and the quad pins stay -1 (same as createChannel())
    detail::SpiBusConfig bus_config(pin, -1, static_cast<int>(staging_size));  // fl-lint: int-template-ok ESP-IDF max_transfer_sz is int
    bus_config.flags = 0;

    if (!mPeripheral.initializeBus(bus_config)) {
        FL_WARN("ChannelEngineSpi: reinit spi_bus_initialize failed");
        releaseSpiHost(state->spi_host);
        state->spi_host = kNoSpiHost;
        return false;
    }

    // Mode 0, no CS pin, queue size 4, no post_cb (same as createChannel())
    detail::SpiDeviceConfig dev_config(static_cast<int>(spi_clock_hz), 4, 0);  // fl-lint: int-template-ok ESP-IDF clock_speed_hz is int
    dev_config.flags = (state->numLanes >= 2) ? detail::kSpiDeviceHalfDuplex : 0;

    if (!mPeripheral.addDevice(dev_config)) {
        FL_WARN("ChannelEngineSpi: reinit spi_bus_add_device failed");
        mPeripheral.freeBus();
        releaseSpiHost(state->spi_host);
        state->spi_host = kNoSpiHost;
        return false;
    }
    state->hasDevice = true;

    // NOTE: Do NOT call gpio_set_drive_capability() here (see createChannel())

    state->pinRouting = mPeripheral.getPinRouting(pin);
    FL_DBG_EVERY(100, "ChannelEngineSpi: Reinit SPI hardware for pin " << pin << " host=" << (static_cast<int>(state->spi_host)));
    return true;
}

bool ChannelEngineSpi::createChannel(SpiChannelState *state, int pin,
                                     const ChipsetTimingConfig &timing,
                                     size_t dataSize) FL_NO_EXCEPT {
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
    FL_DBG_EVERY(10, "ChannelEngineSpi: Creating channel for pin " << pin << " (attempt " << creation_count << " in last 5s)");

    if (creation_count > 100) {
        FL_ERROR("ChannelEngineSpi: ABORT - Too many channel creation attempts (" << creation_count << " in 5s). Possible infinite loop or resource leak.");
        return false;
    }

    // Acquire SPI host
    state->spi_host = acquireSpiHost();
    if (state->spi_host == kNoSpiHost) {
        FL_WARN_EVERY(10, "ChannelEngineSpi: No available SPI host (attempt " << creation_count << ")");
        return false;
    }

    // Staging buffer size determines max SPI transaction size. 16 KB lets a
    // single ESP-IDF spi_device_queue_trans() produce continuous wave8 output
    // for up to ~680 WS2812B LEDs. With the earlier 4 KB size, any strip >
    // ~170 LEDs was split across multiple poll()-driven transactions with
    // inter-chunk gaps long enough for WS2812B to latch - the "black frame
    // between displays" bug reported in #2254. 16 KB matches the per-channel
    // internal-SRAM cost (2x double-buffered = 32 KB) - tight but acceptable
    // on ESP32-S3 (~320 KB internal SRAM).
    const size_t staging_size = 16384;

    // Determine if we should use DMA
    // Use DMA for larger buffers (>64 bytes)
    const size_t spiBufferSize = dataSize * WAVE8_BYTES_PER_COLOR_BYTE;
    state->useDMA = (spiBufferSize > 64);

    // Reset GPIO to default state before SPI initialization.
    // On ESP32-C6, GPIO 0-6 are RTC GPIOs (XTAL_32K_P, etc.) that may be in
    // analog/RTC mode at boot. gpio_reset_pin() disconnects them from RTC/analog
    // functions so spi_bus_initialize() can properly route the SPI MOSI signal
    // through the GPIO matrix. Without this, SPI MOSI may fail to produce output
    // on these pins even though spi_bus_initialize() returns ESP_OK.
    mPeripheral.resetPin(pin);

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
    // MOSI = data pin (only pin connected to LED strip). MISO and SCLK = -1
    // (match Espressif led_strip: clock internal only). Quad WP/HD = -1.
    // Chunked transmission: max transfer = staging buffer size.
    detail::SpiBusConfig bus_config(pin, -1, static_cast<int>(staging_size));  // fl-lint: int-template-ok ESP-IDF max_transfer_sz is int

    FL_DBG("ChannelEngineSpi: SPI bus config (Espressif led_strip pattern) - MOSI=" << (static_cast<int>(pin)) << ", SCLK=-1 (internal), MISO=-1 (unused), host=" << (static_cast<int>(state->spi_host)));

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
    if (!mPeripheral.initializeBus(bus_config)) {
        FL_WARN("ChannelEngineSpi: spi_bus_initialize failed");
        releaseSpiHost(state->spi_host);
        state->spi_host = kNoSpiHost;
        return false;
    }

    // Configure SPI device
    // Calculate SPI clock from chipset timing for wave8 encoding:
    // Wave8 expands each LED bit to 8 SPI bits, so SPI clock = 8 / bit_period
    // For WS2812 (1250ns period): 8e9 / 1250 = 6.4 MHz
    const u32 total_period_ns = timing.total_period_ns();
    const u32 spi_clock_hz = calculateWave8SpiClockHz(total_period_ns > 0 ? total_period_ns : 1250);
    FL_DBG("ChannelEngineSpi: Wave8 SPI clock: " << spi_clock_hz << "Hz (8-bit expansion, period=" << total_period_ns << "ns)");

    // SPI mode 0, no CS pin, queue size 4, default clock source (match
    // Espressif led_strip, ESP-IDF 5.x). Match Espressif led_strip: NO post_cb
    // callback (none is registered with the peripheral).
    // Reference: https://github.com/espressif/idf-extra-components/blob/master/led_strip/src/led_strip_spi_dev.c
    detail::SpiDeviceConfig dev_config(static_cast<int>(spi_clock_hz), 4, 0);  // fl-lint: int-template-ok ESP-IDF clock_speed_hz is int

    FL_DBG("ChannelEngineSpi: SPI clock_hz=" << spi_clock_hz << " (wave8 encoding), bits_per_led_bit=8 (wave8), buffer_size=" << spiBufferSize << " bytes");

    // Device flags configuration:
    // Match Espressif led_strip pattern: minimal flags
    // Reference: https://github.com/espressif/idf-extra-components/blob/master/led_strip/src/led_strip_spi_dev.c
    // Espressif led_strip uses NO flags (dev_config.flags = 0 implicitly)
    // - SPI_DEVICE_HALFDUPLEX: Required for multi-lane (dual/quad) mode per ESP-IDF docs.
    if (state->numLanes >= 2) {
        // Multi-lane mode REQUIRES HALFDUPLEX flag per ESP-IDF documentation
        dev_config.flags = detail::kSpiDeviceHalfDuplex;
        FL_DBG("ChannelEngineSpi: Multi-lane mode (" << (static_cast<int>(state->numLanes)) << " lanes) - using SPI_DEVICE_HALFDUPLEX");
    } else {
        // Single-lane: match Espressif led_strip (no flags)
        dev_config.flags = 0;
        FL_DBG("ChannelEngineSpi: Single-lane mode - no device flags (matches Espressif led_strip)");
    }

    // Add device to bus
    if (!mPeripheral.addDevice(dev_config)) {
        FL_WARN("ChannelEngineSpi: spi_bus_add_device failed");
        mPeripheral.freeBus();
        releaseSpiHost(state->spi_host);
        state->spi_host = kNoSpiHost;
        return false;
    }
    state->hasDevice = true;
    state->pinRouting = mPeripheral.getPinRouting(pin);

    // Verify actual clock frequency (tolerance: +/-500 kHz for wave8)
    const int actual_freq_khz = mPeripheral.getActualFrequencyKhz();
    const int requested_freq_khz = static_cast<int>(spi_clock_hz / 1000);
    FL_DBG("ChannelEngineSpi: Actual SPI clock frequency: " << actual_freq_khz << " kHz (requested " << requested_freq_khz << " kHz)");
    if (actual_freq_khz < requested_freq_khz - 500 ||
        actual_freq_khz > requested_freq_khz + 500) {
        FL_WARN_ONCE("ChannelEngineSpi: Clock frequency mismatch - requested " << requested_freq_khz << " kHz, actual " << actual_freq_khz << " kHz (tolerance: +/-500kHz)");
    }

    // NOTE: Do NOT call gpio_set_drive_capability() here.
    // On ESP32-C6, calling gpio_set_drive_capability() after spi_bus_initialize()
    // disrupts the SPI GPIO routing configured by the driver, causing MOSI to
    // stop outputting data. spi_bus_initialize() handles all GPIO configuration.
    // Matches Espressif's led_strip driver which does not modify GPIO after init.

    // Allocate staging buffers (4KB each, double-buffered)
    // Following Espressif's led_strip driver pattern:
    // - MALLOC_CAP_INTERNAL: Forces allocation in internal SRAM
    // - MALLOC_CAP_DMA: Ensures DMA-accessible memory alignment
    // Using internal DMA memory eliminates cache coherence issues.
    //
    // Double-buffering allows overlapping DMA transfer with CPU encoding:
    //   - Encode into buffer A while DMA sends buffer B (and vice versa)
    //   - Reduces wall-clock time by overlapping encode + transfer

    state->stagingA = mPeripheral.allocateDma(staging_size);
    state->stagingB = mPeripheral.allocateDma(staging_size);

    if (!state->stagingA || !state->stagingB) {
        FL_WARN("ChannelEngineSpi: Failed to allocate staging buffers");
        if (state->stagingA) { mPeripheral.freeDma(state->stagingA); state->stagingA = nullptr; }
        if (state->stagingB) { mPeripheral.freeDma(state->stagingB); state->stagingB = nullptr; }
        mPeripheral.removeDevice();
        state->hasDevice = false;
        mPeripheral.freeBus();
        releaseSpiHost(state->spi_host);
        return false;
    }

    // Zero both staging buffers
    fl::memset(state->stagingA, 0, staging_size);
    fl::memset(state->stagingB, 0, staging_size);

    FL_DBG("ChannelEngineSpi: Allocated double staging buffers via allocateDma (INTERNAL|DMA)");

    state->stagingCapacity = staging_size;
    state->currentStaging = state->stagingA;
    state->stagingOffset = 0;

    // Allocate LED source buffer (internal SRAM for ISR-safe access)
    // CRITICAL: PSRAM is NOT safe for ISR access - we must copy LED data here
    // Allocate for typical LED strip size (dataSize is passed from channel data)
    state->ledSourceBuffer = mPeripheral.allocateInternal(dataSize);
    state->ledSourceBufferSize = state->ledSourceBuffer ? dataSize : 0;

    if (!state->ledSourceBuffer) {
        FL_WARN("ChannelEngineSpi: Failed to allocate LED source buffer (" << dataSize << " bytes) - falling back to direct access");
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
    state->transA = detail::SpiTransaction();
    state->transB = detail::SpiTransaction();

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

    FL_DBG_EVERY(10, "ChannelEngineSpi: Created pin=" << pin << " lanes=" << (static_cast<int>(state->numLanes)) << " host=" << state->spi_host);

    return true;
}
int ChannelEngineSpi::acquireSpiHost() FL_NO_EXCEPT {
    // POLICY: Single SPI host (SPI2_HOST), selected by the peripheral.
    //
    // ESP32-S3 FSPI (SPI2_HOST) has known DMA issues when MISO=-1:
    // - DMA RX/TX enable bits in SPI_DMA_CONF_REG affect operation even for TX-only
    // - Reference: https://github.com/Bodmer/TFT_eSPI/discussions/2233
    // Following Espressif led_strip pattern, SPI2 (FSPI) is used on every
    // variant; SPI3 is not used, and SPI1 is reserved for flash.
    //
    // Multi-lane SPI (dual/quad) is still supported on the SINGLE host.
    // Sequential channel transmission through batching handles multiple strips.
    const int host = kSpiHost;
    SpiHostTracking *tracking = &sSpiHostUsage;

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
        FL_DBG("ChannelEngineSpi: Acquired SPI host " << host << " (refCount=" << tracking->refCount << ")");
        return host;
    }

    // Single SPI host policy: Only one SPI host is used
    // If we get here, the preferred host is already in use - this is expected behavior
    // for multiple channels. The caller will queue the channel for later.
    FL_DBG_EVERY(10, "ChannelEngineSpi: SPI host in use, channel will be queued for sequential transmission");
    return kNoSpiHost;
}

void ChannelEngineSpi::releaseSpiHost(int host) FL_NO_EXCEPT {
    // Invalid or unacquired hosts remain a no-op, including kNoSpiHost.
    if (host != kSpiHost) {
        return;
    }
    auto &entry = sSpiHostUsage;
    if (entry.refCount > 0) {
        entry.refCount--;
        FL_DBG("ChannelEngineSpi: Released SPI host " << host << " (refCount=" << entry.refCount << ")");
        if (entry.refCount == 0) {
            mPeripheral.freeBus();
            entry.initialized = false;
            FL_DBG("ChannelEngineSpi: Freed SPI bus " << host);
        }
    }
}

void ChannelEngineSpi::processPendingChannels() FL_NO_EXCEPT {
    // Try to process pending channels now that hardware may be available
    fl::vector_inlined<PendingChannel, 16> stillPending;

    for (auto &pending : mPendingChannels) {
        int pin = pending.pin;
        const auto &ledData = pending.data->getData();

        SpiChannelState *channel =
            acquireChannel(pin, pending.timing, ledData.size());
        if (!channel) {
            // Still no hardware - increment retry count
            pending.retry_count++;

            // Give up after 50 failed attempts to prevent infinite retry storms
            if (pending.retry_count > 50) {
                FL_WARN_ONCE("ChannelEngineSpi: Giving up on pending channel for pin " << pin << " after " << pending.retry_count << " failed attempts. Possible resource leak or hardware unavailability.");
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

size_t ChannelEngineSpi::encodeChunk(SpiChannelState* channel, u8* output, size_t output_capacity) FL_NO_EXCEPT {
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
    const size_t bytes_to_encode = min(channel->ledBytesRemaining, max_led_bytes);

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
        const size_t reset_bytes = min(RESET_ZERO_BYTES, available);
        if (reset_bytes > 0) {
            fl::memset(output + total_bytes_written, 0, reset_bytes);
            total_bytes_written += reset_bytes;
        }
    }

    FL_MEMORY_BARRIER;
    return total_bytes_written;
}

void ChannelEngineSpi::transmitStreaming(SpiChannelState* channel) FL_NO_EXCEPT {
    // Synchronous streaming: encode chunks and transmit via blocking SPI DMA.
    if (!channel || !channel->ledSource || channel->ledBytesRemaining == 0) {
        return;
    }

    while (channel->ledBytesRemaining > 0) {
        size_t encoded = encodeChunk(channel, channel->currentStaging, channel->stagingCapacity);
        if (encoded == 0) {
            break;
        }

        detail::SpiTransaction trans;
        setupTransaction(&trans, channel->currentStaging, encoded, channel->numLanes);

        // spi_device_polling_transmit() == polling start + blocking end
        if (!mPeripheral.pollingStart(trans) ||
            !mPeripheral.pollingEnd(detail::kSpiWaitForeverMs)) {
            break;
        }
    }
    channel->stagingOffset = 0;
}

bool ChannelEngineSpi::startNextChannel() FL_NO_EXCEPT {
    // Walk through timing groups and find the next channel to transmit
    while (mPipeline.mCurrentTimingGroup < mPipeline.mTimingGroups.size()) {
        auto& group = mPipeline.mTimingGroups[mPipeline.mCurrentTimingGroup];

        while (mPipeline.mCurrentChannelInBatch < group.channels.size()) {
            auto& data = group.channels[mPipeline.mCurrentChannelInBatch];
            mPipeline.mCurrentChannelInBatch++;

            int pin = data->getPin();
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

void ChannelEngineSpi::startFirstDma() FL_NO_EXCEPT {
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
    detail::SpiTransaction* trans = &ch->transA;
    setupTransaction(trans, ch->stagingA, encoded, ch->numLanes);

    if (spiStart(mPeripheral, *trans)) {
        ch->transAInFlight = true;
        mPipeline.mDmaInFlight = true;
        mPipeline.mEncodeIdx = 1; // Next encode goes to buffer B
        mPipeline.mPhase = DmaPipelineState::STREAMING;

        // Back-to-back DMA streaming (issue #2304):
        //
        // For long strips (>~680 LEDs) the encoded SPI data spans more than one
        // staging buffer. The naive approach (queue chunk A, wait, queue B,
        // wait, ...) leaves the SPI line idle for the poll() latency between
        // chunks. Under RTOS contention that gap can exceed WS2812B's ~50us
        // reset threshold and trigger a visible refresh.
        //
        // Fix: when async DMA is available, pre-encode and pre-queue the
        // second chunk right away so the SPI ISR transitions directly from
        // transA -> transB without host involvement. ESP-IDF's
        // spi_device_queue_trans permits multiple in-flight transactions
        // (dev_config.queue_size = 4 already in place); we cap at 2 to keep
        // the pipeline shallow and predictable.
        //
        // ESP32-C6 split-polling path: spi_device_polling_start only allows
        // one in-flight polling transaction at a time, so we keep the
        // existing single-queue behaviour there. The C6 still benefits from
        // the encode/wait overlap but cannot get the ISR-chained gap-free
        // behaviour without an ESP-IDF API change.
        if (!kSpiUsePolling && ch->ledBytesRemaining > 0) {
            // Save source state so we can roll back if the SPI driver refuses
            // to queue transB. encodeChunk() advances ch->ledSource /
            // ch->ledBytesRemaining as a side-effect, so a failure here would
            // otherwise drop the chunk we just consumed instead of letting the
            // refill loop retry it on the next poll() iteration.
            const u8* savedSource = ch->ledSource;
            size_t savedRemaining = ch->ledBytesRemaining;

            size_t encoded_b = encodeChunk(ch, ch->stagingB, ch->stagingCapacity);
            if (encoded_b > 0) {
                detail::SpiTransaction* transB = &ch->transB;
                setupTransaction(transB, ch->stagingB, encoded_b, ch->numLanes);

                if (spiStart(mPeripheral, *transB)) {
                    ch->transBInFlight = true;
                    mPipeline.mEncodeIdx = 2; // Both A and B queued
                } else {
                    // Pre-queue failed but A is already in flight. Roll back
                    // the source pointer so the refill loop re-encodes this
                    // chunk into B once A completes instead of truncating the
                    // stream after A.
                    ch->ledSource = savedSource;
                    ch->ledBytesRemaining = savedRemaining;
                    FL_WARN("ChannelEngineSpi: startFirstDma pre-queue B failed");
                }
            }
        }
    } else {
        FL_WARN("ChannelEngineSpi: startFirstDma spiStart failed");
        ch->transmissionComplete = true;
        mPipeline.mPhase = DmaPipelineState::IDLE;
    }
}

IChannelDriver::DriverState ChannelEngineSpi::advancePipeline() FL_NO_EXCEPT {
    auto* ch = mPipeline.mActiveChannel;

    switch (mPipeline.mPhase) {
    case DmaPipelineState::IDLE:
        return DriverState::READY;

    case DmaPipelineState::STREAMING: {
        if (!ch) {
            mPipeline.mPhase = DmaPipelineState::IDLE;
            return DriverState::READY;
        }

        // "Any chunk in flight" - refreshed each iteration. Tracks the real
        // state of both transA and transB so that pre-queued back-to-back
        // chunks are accounted for (issue #2304).
        bool anyInFlight = ch->transAInFlight || ch->transBInFlight;

        // Check if a DMA has completed. In async mode this drains the next
        // completed transaction from ESP-IDF's FIFO queue; in polling mode
        // it ends the current (single) polling transaction.
        if (anyInFlight) {
            void* completed = nullptr;
            if (!spiCheckDone(mPeripheral, &completed)) {
                return DriverState::DRAINING; // DMA still in flight
            }
            if (kSpiUsePolling) {
                // Polling mode: polling_end doesn't return a transaction pointer,
                // determine which buffer completed from mEncodeIdx
                int lastBuf = (mPipeline.mEncodeIdx - 1) & 1;
                if (lastBuf == 0) ch->transAInFlight = false;
                else ch->transBInFlight = false;
            } else {
                if (completed == &ch->transA) ch->transAInFlight = false;
                else if (completed == &ch->transB) ch->transBInFlight = false;
            }
            // Re-evaluate after draining one completion
            anyInFlight = ch->transAInFlight || ch->transBInFlight;
            mPipeline.mDmaInFlight = anyInFlight;
        }

        // Encode and queue next chunk if data remains. After the back-to-back
        // pre-queue in startFirstDma(), this fires on each subsequent
        // completion and refills the buffer that just freed up - keeping
        // two transactions in the SPI ISR's queue so it can chain them
        // gap-free.
        if (ch->ledBytesRemaining > 0) {
            int bufIdx = mPipeline.mEncodeIdx & 1;
            u8* buffer = (bufIdx == 0) ? ch->stagingA : ch->stagingB;
            detail::SpiTransaction* trans = (bufIdx == 0) ? &ch->transA : &ch->transB;

            size_t encoded = encodeChunk(ch, buffer, ch->stagingCapacity);
            if (encoded > 0) {
                setupTransaction(trans, buffer, encoded, ch->numLanes);

                if (spiStart(mPeripheral, *trans)) {
                    if (bufIdx == 0) ch->transAInFlight = true;
                    else ch->transBInFlight = true;
                    mPipeline.mDmaInFlight = true;
                    mPipeline.mEncodeIdx++;
                }
            }
            return DriverState::DRAINING;
        }

        // No more data to encode - drain any chunks still in flight before
        // marking the channel complete.
        if (anyInFlight) {
            mPipeline.mPhase = DmaPipelineState::COMPLETING;
            return DriverState::DRAINING;
        }

        // All done for this channel
        ch->transmissionComplete = true;
        ch->stagingOffset = 0;
        releaseChannel(ch);

        if (startNextChannel()) {
            startFirstDma();
            return DriverState::DRAINING;
        }
        mPipeline.mPhase = DmaPipelineState::IDLE;
        mPipeline.mActiveChannel = nullptr;
        return DriverState::READY;
    }

    case DmaPipelineState::COMPLETING: {
        if (!ch) {
            mPipeline.mPhase = DmaPipelineState::IDLE;
            return DriverState::READY;
        }

        // Wait for final DMA to complete
        if (ch->transAInFlight || ch->transBInFlight) {
            void* completed = nullptr;
            if (!spiCheckDone(mPeripheral, &completed)) return DriverState::DRAINING;
            if (kSpiUsePolling) {
                int lastBuf = (mPipeline.mEncodeIdx - 1) & 1;
                if (lastBuf == 0) ch->transAInFlight = false;
                else ch->transBInFlight = false;
            } else {
                if (completed == &ch->transA) ch->transAInFlight = false;
                else if (completed == &ch->transB) ch->transBInFlight = false;
            }
            if (ch->transAInFlight || ch->transBInFlight) return DriverState::DRAINING;
        }

        ch->transmissionComplete = true;
        ch->stagingOffset = 0;
        releaseChannel(ch);

        if (startNextChannel()) {
            startFirstDma();
            return DriverState::DRAINING;
        }
        mPipeline.mPhase = DmaPipelineState::IDLE;
        mPipeline.mActiveChannel = nullptr;
        return DriverState::READY;
    }

    case DmaPipelineState::RESET_DELAY: {
        u32 now = fl::micros();
        if ((now - mPipeline.mResetDelayStartUs) >= mPipeline.mResetDelayDurationUs) {
            if (startNextChannel()) {
                startFirstDma();
                return DriverState::DRAINING;
            }
            mPipeline.mPhase = DmaPipelineState::IDLE;
            mPipeline.mActiveChannel = nullptr;
            return DriverState::READY;
        }
        return DriverState::DRAINING;
    }
    }

    return DriverState::READY;
}

} // namespace fl

#endif // FASTLED_ESP32_HAS_CLOCKLESS_SPI or FASTLED_STUB_IMPL

#endif // FL_IS_ESP32 or FASTLED_STUB_IMPL
