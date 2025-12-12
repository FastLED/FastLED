/// @file channel_engine_parlio.cpp
/// @brief Parallel IO implementation of ChannelEngine for ESP32-P4/C6/H2/C5
///
/// This implementation uses ESP32's Parallel IO (PARLIO) peripheral to drive
/// multiple LED strips simultaneously on parallel GPIO pins. It supports
/// ESP32-P4, ESP32-C6, ESP32-H2, and ESP32-C5 variants that have PARLIO
/// hardware. Note: ESP32-S3 does NOT have PARLIO (it has LCD peripheral
/// instead).
///
/// This is a runtime-configurable implementation supporting 1-16 channels with
/// power-of-2 data widths (1, 2, 4, 8, 16) determined at construction time.

#ifdef ESP32

#include "fl/compiler_control.h"
#include "platforms/esp/32/feature_flags/enabled.h"

#if FASTLED_ESP32_HAS_PARLIO

#include "channel_engine_parlio.h"
#include "fl/delay.h"
#include "fl/log.h"
#include "fl/transposition.h"
#include "fl/warn.h"
#include "fl/error.h"
#include "ftl/algorithm.h"
#include "ftl/assert.h"
#include "ftl/time.h"
#include "platforms/esp/32/core/fastpin_esp32.h" // For _FL_VALID_PIN_MASK

FL_EXTERN_C_BEGIN
#include "driver/gpio.h"
#include "driver/parlio_tx.h"
#include "esp_heap_caps.h" // For DMA-capable memory allocation
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "soc/soc_caps.h"  // For SOC_PARLIO_TX_UNIT_MAX_DATA_WIDTH
FL_EXTERN_C_END

#ifndef CONFIG_PARLIO_TX_ISR_HANDLER_IN_IRAM
    #warning "PARLIO: CONFIG_PARLIO_TX_ISR_HANDLER_IN_IRAM is not defined! Add 'build_flags = -DCONFIG_PARLIO_TX_ISR_HANDLER_IN_IRAM=1' to platformio.ini or your build system"
#endif

namespace fl {

//=============================================================================
// Constants
//=============================================================================

// WS2812 timing requirements with PARLIO
// Clock: 8.0 MHz (125ns per tick)
// Divides from PLL_F160M on ESP32-P4 (160/20) or PLL_F240M on ESP32-C6 (240/30)
// Each LED bit is encoded as 10 clock ticks (1.25μs total)
static constexpr uint32_t PARLIO_CLOCK_FREQ_HZ = 8000000; // 8.0 MHz

// ITERATION 3: ISR callback debug counter (ISR-safe)
static volatile uint32_t g_parlio_isr_callback_count = 0;

//=============================================================================
// Pin Validation Using FastLED's _FL_PIN_VALID System
//=============================================================================
// PARLIO no longer uses default pins. Instead, pins are extracted from
// ChannelData objects and validated using the FastLED pin validation system
// defined in platforms/esp/32/core/fastpin_esp32.h.
//
// The _FL_PIN_VALID macro checks against:
// 1. SOC_GPIO_VALID_OUTPUT_GPIO_MASK (ESP-IDF's valid output pins)
// 2. FASTLED_UNUSABLE_PIN_MASK (platform-specific forbidden pins)
//
// Pins are provided by user via FastLED.addLeds<WS2812, PIN>() API.
//=============================================================================

// Import pin validation mask from fastpin_esp32.h
// This is defined per-platform in src/platforms/esp/32/core/fastpin_esp32.h
#ifndef _FL_VALID_PIN_MASK
#error "Pin validation system not available - check fastpin_esp32.h is included"
#endif

/// @brief Validate a GPIO pin for PARLIO use
/// @param pin GPIO pin number to validate
/// @return true if pin is valid for PARLIO output, false otherwise
static inline bool isParlioPinValid(int pin) {
    if (pin < 0 || pin >= 64) {
        return false;
    }
    // Use FastLED's pin validation system
    uint64_t pin_mask = (1ULL << pin);
    return (_FL_VALID_PIN_MASK & pin_mask) != 0;
}

//=============================================================================
// Helper Functions
//=============================================================================

//-----------------------------------------------------------------------------
// ISR Transposition Algorithm Design
//-----------------------------------------------------------------------------
//
// PARLIO hardware transmits data in parallel across multiple GPIO pins.
// For WS2812 LEDs, we use the generic waveform generator
// (fl::channels::waveform_generator) with custom bit-packing for PARLIO's
// parallel format.
//
// INPUT FORMAT (per-strip layout):
//   Strip 0: [LED0_R, LED0_G, LED0_B, LED1_R, LED1_G, LED1_B, ...]
//   Strip 1: [LED0_R, LED0_G, LED0_B, LED1_R, LED1_G, LED1_B, ...]
//   ...
//   Strip 7: [LED0_R, LED0_G, LED0_B, LED1_R, LED1_G, LED1_B, ...]
//
// ISR PROCESSING (per LED byte):
//   For each color byte position:
//     1. Read byte from all 8 lanes (per-strip layout with stride)
//     2. Expand each byte using fl::expandByteToWaveforms() with precomputed
//     bit0/bit1 patterns
//        - Each byte → 32 pulse bytes (0xFF=HIGH, 0x00=LOW)
//     3. Bit-pack transpose: convert pulse bytes to PARLIO's bit-packed format
//        - Each pulse byte → 1 bit, packed across lanes
//
// OUTPUT FORMAT (bit-parallel, 8 lanes):
//   Byte 0:  [S7_p0, S6_p0, ..., S1_p0, S0_p0]  // Pulse 0 from all strips
//   Byte 1:  [S7_p1, S6_p1, ..., S1_p1, S0_p1]  // Pulse 1 from all strips
//   ...
//   Byte 31: [S7_p31, S6_p31, ..., S1_p31, S0_p31]  // Pulse 31 from all strips
//   (Pattern repeats for each color of each LED)
//
// MEMORY LAYOUT:
//   For 8 lanes:
//     - Each LED byte → 32 ticks × 1 byte = 32 bytes (after waveform encoding)
//     - Each LED (RGB) → 96 bytes (after waveform encoding)
//
// BUFFER SIZE CALCULATION:
//   Scratch buffer: N LEDs × 3 colors × 8 strips bytes (per-strip layout)
//   DMA chunks: 100 LEDs × 3 × 32 ticks × 1 byte = 9600 bytes (per buffer)
//
// EXAMPLE (1000 LEDs, 8 strips):
//   Scratch buffer: 1000 × 3 × 8 = 24,000 bytes (per-strip layout)
//   DMA buffers: 2 × (100 × 3 × 32 × 1) = 19,200 bytes (ping-pong expanded
//   waveforms) Total: ~43 KB (waveform expansion happens in ISR, not stored)
//
// CHUNKING STRATEGY:
//   PARLIO has 65535 byte max transfer size. For large LED counts:
//   - Break transmission at LED boundaries (not in middle of RGB)
//   - Fixed chunk size: 100 LEDs (9600 bytes per chunk)
//   - Max ~682 LEDs per chunk (65535 / 96 bytes per LED)
//   - Transmit chunks sequentially via ping-pong DMA buffering
//
// WAVEFORM GENERATION:
//   Uses generic waveform generator with WS2812 timing parameters:
//   - T1: 375ns (initial HIGH time)
//   - T2: 500ns (additional HIGH time for bit 1)
//   - T3: 375ns (LOW tail duration)
//   - Clock: 8.0 MHz (125ns per pulse)
//   - Result: 10 pulses per bit (bit0=3H+7L, bit1=7H+3L)
//
//-----------------------------------------------------------------------------

//=============================================================================
// Constructors / Destructors - Implementation Class
//=============================================================================

ChannelEnginePARLIOImpl::ChannelEnginePARLIOImpl(size_t data_width)
    : mInitialized(false), mState(data_width) {
    // Validate data width
    if (data_width != 1 && data_width != 2 && data_width != 4 &&
        data_width != 8 && data_width != 16) {
        FL_WARN("PARLIO: Invalid data_width="
                << data_width << " (must be 1, 2, 4, 8, or 16)");
        // Constructor will still complete, but initialization will fail
        return;
    }

    FL_LOG_PARLIO("PARLIO Channel Engine initialized (data_width=" << data_width
                                                                   << ")");
}

ChannelEnginePARLIOImpl::~ChannelEnginePARLIOImpl() {
    // Wait for any active transmissions to complete
    // Note: delayMicroseconds already yields to watchdog
    while (poll() == EngineState::BUSY) {
        fl::delayMicroseconds(100);
    }

    // Clean up PARLIO TX unit resources
    if (mState.tx_unit != nullptr) {
        FL_LOG_PARLIO("PARLIO: Cleaning up TX unit");

        // Wait for any pending transmissions (with timeout)
        esp_err_t err =
            parlio_tx_unit_wait_all_done(mState.tx_unit, pdMS_TO_TICKS(1000));
        if (err != ESP_OK) {
            FL_WARN("PARLIO: Wait for transmission timeout during cleanup: "
                    << err);
        }

        // Disable TX unit
        err = parlio_tx_unit_disable(mState.tx_unit);
        if (err != ESP_OK) {
            FL_WARN("PARLIO: Failed to disable TX unit: " << err);
        }

        // Delete TX unit
        err = parlio_del_tx_unit(mState.tx_unit);
        if (err != ESP_OK) {
            FL_WARN("PARLIO: Failed to delete TX unit: " << err);
        }

        mState.tx_unit = nullptr;
        FL_LOG_PARLIO("PARLIO: TX unit cleanup complete");
    }

    // DMA buffers and waveform expansion buffer are automatically freed
    // by fl::unique_ptr destructors (RAII)

    // Clear state
    mState.pins.clear();
    mState.scratch_padded_buffer.clear();
    mState.buffer_size = 0;
    mState.num_lanes = 0;
    mState.lane_stride = 0;
    mState.transmitting = false;
    mState.stream_complete = false;
    mState.error_occurred = false;

    FL_LOG_PARLIO("PARLIO: Channel Engine destroyed");
}

//=============================================================================
// Private Methods - ISR Streaming Support
//=============================================================================

bool IRAM_ATTR ChannelEnginePARLIOImpl::txDoneCallback(
    parlio_tx_unit_handle_t tx_unit, const void *edata, void *user_ctx) {
    // ISR context - must be fast and IRAM-safe
    // Note: edata is actually const parlio_tx_done_event_data_t* but we don't
    // use it
    (void)edata;   // Unused parameter
    (void)tx_unit; // Unused parameter
    auto *self = static_cast<ChannelEnginePARLIOImpl *>(user_ctx);
    if (!self) {
        return false;
    }

    // ITERATION 3: Increment ISR callback counter (ISR-safe)
    // Use atomic operation to avoid volatile increment warning
    __atomic_add_fetch(&g_parlio_isr_callback_count, 1, __ATOMIC_RELAXED);

    // Check if there are more chunks to transmit
    if (self->mState.current_led >= self->mState.total_leds) {
        // Transmission complete
        self->mState.stream_complete = true;
        self->mState.transmitting = false;
        return false;
    }

    // Transpose and queue next chunk
    if (!self->transposeAndQueueNextChunk()) {
        self->mState.error_occurred = true;
        self->mState.transmitting = false;
        return false;
    }

    return false; // No high-priority task woken
}

bool IRAM_ATTR ChannelEnginePARLIOImpl::transposeAndQueueNextChunk() {
    // ISR-OPTIMIZED: Generic waveform expansion + bit-pack transposition
    // Uses fl::expandByteToWaveforms() with precomputed bit0/bit1 patterns
    // Transposes byte-based waveforms to PARLIO's bit-packed format

    // Calculate chunk parameters
    size_t ledsRemaining = mState.total_leds - mState.current_led;
    size_t ledsThisChunk = (ledsRemaining < mState.leds_per_chunk)
                               ? ledsRemaining
                               : mState.leds_per_chunk;

    if (ledsThisChunk == 0) {
        return true; // Nothing to do
    }

    // Get the next buffer to fill
    uint8_t *nextBuffer = const_cast<uint8_t *>(mState.fill_buffer);

    // Zero output buffer to prevent garbage data from previous use
    // (heap_caps_malloc does not zero memory, unlike calloc)
    fl::memset(nextBuffer, 0x00, mState.buffer_size);

    // DEBUG: Validate buffer pointer before use
    if (nextBuffer == nullptr) {
        // FL_WARN("PARLIO: BUG - fill_buffer is NULL! (buffer_a=" <<
        // (void*)mState.buffer_a
        //         << ", buffer_b=" << (void*)mState.buffer_b
        //         << ", active_buffer=" << (void*)mState.active_buffer << ")");
        return false;
    }

    // FL_LOG_PARLIO("PARLIO: Transposing chunk (LED " << mState.current_led <<
    // "-"
    //               << (mState.current_led + ledsThisChunk - 1) << " of " <<
    //               mState.total_leds
    //               << ", buffer=" << (void*)nextBuffer << ")");

    // Use pre-allocated waveform expansion buffer to avoid heap alloc in IRAM
    // CRITICAL: This buffer MUST be heap-allocated (not stack) because it's
    // accessed from ISR context and may be large (up to 2KB for 16 lanes × 128
    // bytes/lane). Buffer was allocated during initialization in
    // initializeIfNeeded() using heap_caps_malloc(). Each byte expands to 8
    // bits × pulses_per_bit For WS2812 at 8MHz: 10 pulses/bit → 8 bits × 10 =
    // 80 bytes per lane
    const size_t max_pulses_per_bit =
        16; // Conservative max (10MHz / 800kHz = 12.5, rounded up)
    const size_t bytes_per_lane = 8 * max_pulses_per_bit; // 128 bytes per lane
    const size_t waveform_buffer_size =
        mState.data_width * bytes_per_lane; // 16 lanes * 128 = 2048 bytes

    // Validate pre-allocated buffer exists and is large enough
    if (!mState.waveform_expansion_buffer ||
        mState.waveform_expansion_buffer_size < waveform_buffer_size) {
        // Cannot use FL_WARN in IRAM function - just fail silently
        mState.error_occurred = true;
        return false;
    }

    uint8_t *laneWaveforms = mState.waveform_expansion_buffer.get();

    // Precomputed waveform pointers
    const uint8_t *bit0_wave = mState.bit0_waveform.data();
    const uint8_t *bit1_wave = mState.bit1_waveform.data();
    size_t waveform_size = mState.pulses_per_bit;

    // NOTE: Cannot use FL_DBG in IRAM function - it allocates heap memory via
    // StrStream

    // Zero waveform buffer to eliminate leftover data from previous iterations
    fl::memset(laneWaveforms, 0x00, waveform_buffer_size);

    size_t outputIdx = 0;

    // Process each LED in this chunk
    for (size_t ledIdx = 0; ledIdx < ledsThisChunk; ledIdx++) {
        size_t absoluteLedIdx = mState.current_led + ledIdx;

        // Process each color channel (R, G, B)
        for (size_t colorIdx = 0; colorIdx < 3; colorIdx++) {
            size_t byteOffset = absoluteLedIdx * 3 + colorIdx;

            // Step 1: Expand each lane's byte to waveform using generic
            // function
            for (size_t lane = 0; lane < mState.data_width; lane++) {
                uint8_t *laneWaveform =
                    laneWaveforms +
                    (lane *
                     bytes_per_lane); // Pointer to this lane's waveform buffer

                if (lane < mState.actual_channels) {
                    // Real channel - expand waveform from scratch buffer
                    const uint8_t *laneData =
                        mState.scratch_padded_buffer.data() +
                        (lane * mState.lane_stride);
                    uint8_t byte = laneData[byteOffset];

                    // Use generic waveform expansion
                    size_t expanded = fl::expandByteToWaveforms(
                        byte, bit0_wave, waveform_size, bit1_wave,
                        waveform_size, laneWaveform, bytes_per_lane);
                    if (expanded == 0) {
                        // Waveform expansion failed - abort
                        mState.error_occurred = true;
                        // CRITICAL: laneWaveforms points to
                        // mState.waveform_expansion_buffer (pre-allocated heap
                        // buffer managed by this class). DO NOT call
                        // heap_caps_free(laneWaveforms)! The buffer is owned by
                        // mState and will be freed in the destructor.
                        return false;
                    }
                } else {
                    // Dummy lane - zero waveform (keeps GPIO LOW)
                    fl::memset(laneWaveform, 0x00, bytes_per_lane);
                }
            }

            // Step 2: Bit-pack transpose waveform bytes to PARLIO format
            // Convert byte-based waveforms (0xFF/0x00) to bit-packed output
            //
            // PARLIO Data Format:
            // - For data_width=N, each N bits represent one clock cycle
            // - Bits are packed: bit 0 = lane 0, bit 1 = lane 1, etc.
            // - Each byte contains 8/N clock cycles worth of data
            //
            // Example: data_width=1, 80 pulses
            //   - Need 80 clock cycles = 80 bits = 10 bytes
            //   - Each byte: [tick7, tick6, ..., tick1, tick0]
            const size_t pulsesPerByte =
                8 * mState.pulses_per_bit; // 80 pulses for 8 bits

            if (mState.data_width <= 8) {
                // Pack into single bytes
                // Each byte contains (8 / data_width) clock cycles
                size_t ticksPerByte = 8 / mState.data_width;
                size_t numOutputBytes = (pulsesPerByte + ticksPerByte - 1) / ticksPerByte;

                for (size_t byteIdx = 0; byteIdx < numOutputBytes; byteIdx++) {
                    uint8_t outputByte = 0;

                    // Pack ticksPerByte time ticks into this byte
                    for (size_t t = 0; t < ticksPerByte; t++) {
                        size_t tick = byteIdx * ticksPerByte + t;
                        if (tick >= pulsesPerByte) break; // Avoid overflow

                        // Extract pulses from all lanes at this time tick
                        for (size_t lane = 0; lane < mState.data_width; lane++) {
                            uint8_t *laneWaveform =
                                laneWaveforms + (lane * bytes_per_lane);
                            uint8_t pulse = laneWaveform[tick];
                            uint8_t bit =
                                (pulse != 0) ? 1 : 0; // Convert 0xFF→1, 0x00→0

                            // Bit position: temporal order (tick 0 → bit 0, tick 1 → bit 1, ...)
                            // PARLIO transmits LSB-first (bit 0 of byte sent first)
                            // This gives correct temporal sequence: tick 0, tick 1, ..., tick 7
                            size_t bitPos = t * mState.data_width + lane;
                            outputByte |= (bit << bitPos);
                        }
                    }

                    nextBuffer[outputIdx++] = outputByte;
                }
            } else if (mState.data_width == 16) {
                // Pack into 16-bit words (two bytes per tick)
                // For 16-bit width: 1 clock cycle = 16 bits = 2 bytes
                for (size_t tick = 0; tick < pulsesPerByte; tick++) {
                    uint16_t outputWord = 0;

                    // Extract one pulse from each lane and pack into bits
                    for (size_t lane = 0; lane < 16; lane++) {
                        uint8_t *laneWaveform =
                            laneWaveforms + (lane * bytes_per_lane);
                        uint8_t pulse = laneWaveform[tick];
                        uint8_t bit =
                            (pulse != 0) ? 1 : 0; // Convert 0xFF→1, 0x00→0
                        outputWord |= (bit << lane); // Lane 0 = bit 0, etc.
                    }

                    // Write as two bytes (low byte first for little-endian)
                    nextBuffer[outputIdx++] = outputWord & 0xFF;          // Low byte
                    nextBuffer[outputIdx++] = (outputWord >> 8) & 0xFF;  // High byte
                }
            }
        }
    }

    // Queue transmission of this buffer
    parlio_transmit_config_t tx_config = {};
    tx_config.idle_value = 0x0000;

    size_t chunkBits =
        outputIdx * 8; // PARLIO API requires payload size in bits

    esp_err_t err = parlio_tx_unit_transmit(mState.tx_unit, nextBuffer,
                                            chunkBits, &tx_config);

    if (err != ESP_OK) {
        mState.error_occurred = true;
        // CRITICAL: laneWaveforms points to mState.waveform_expansion_buffer
        // (pre-allocated heap buffer managed by this class). DO NOT call
        // heap_caps_free(laneWaveforms)! The buffer is owned by mState and will
        // be freed in the destructor.
        return false;
    }

    // Update state
    mState.current_led += ledsThisChunk;

    // Swap buffer pointers for ping-pong buffering
    // After first transmission: active=buffer_a, fill=buffer_b
    // After second transmission: active=buffer_b, fill=buffer_a
    mState.active_buffer = mState.fill_buffer;
    if (mState.fill_buffer == mState.buffer_a.get()) {
        mState.fill_buffer = mState.buffer_b.get();
    } else {
        mState.fill_buffer = mState.buffer_a.get();
    }

    // Note: laneWaveforms points to mState.waveform_expansion_buffer
    // (pre-allocated heap buffer). DO NOT free it here - the buffer is managed
    // by mState and freed in the destructor.

    return true;
}

//=============================================================================
// Public Interface - IChannelEngine Implementation
//=============================================================================

void ChannelEnginePARLIOImpl::enqueue(ChannelDataPtr channelData) {
    if (channelData) {
        mEnqueuedChannels.push_back(channelData);
    }
}

void ChannelEnginePARLIOImpl::show() {
    if (!mEnqueuedChannels.empty()) {
        // Move enqueued channels to transmitting channels
        mTransmittingChannels = fl::move(mEnqueuedChannels);
        mEnqueuedChannels.clear();

        // Begin transmission
        beginTransmission(fl::span<const ChannelDataPtr>(
            mTransmittingChannels.data(), mTransmittingChannels.size()));
    }
}

IChannelEngine::EngineState ChannelEnginePARLIOImpl::poll() {
    // If not initialized, we're ready (no hardware to poll)
    if (!mInitialized || mState.tx_unit == nullptr) {
        return EngineState::READY;
    }

    // Check for ISR-reported errors
    if (mState.error_occurred) {
        FL_WARN("PARLIO: Error occurred during streaming transmission");
        mState.transmitting = false;
        mState.error_occurred = false;
        return EngineState::ERROR;
    }

    // Check if streaming is complete
    if (mState.stream_complete) {
        // Wait for final chunk to complete
        esp_err_t err = parlio_tx_unit_wait_all_done(mState.tx_unit, 0);

        if (err == ESP_OK) {
            mState.transmitting = false;
            mState.stream_complete = false;
            // Clear transmitting channels on completion
            mTransmittingChannels.clear();
            FL_LOG_PARLIO(
                "PARLIO: Streaming transmission complete (ISR callbacks: "
                << g_parlio_isr_callback_count << ")");
            return EngineState::READY;
        } else if (err == ESP_ERR_TIMEOUT) {
            // Final chunk still transmitting
            return EngineState::BUSY;
        } else {
            FL_WARN("PARLIO: Error waiting for final chunk: " << err);
            mState.transmitting = false;
            mState.stream_complete = false;
            return EngineState::ERROR;
        }
    }

    // If we're not transmitting, we're ready
    if (!mState.transmitting) {
        return EngineState::READY;
    }

    // Streaming in progress
    return EngineState::BUSY;
}

void ChannelEnginePARLIOImpl::beginTransmission(
    fl::span<const ChannelDataPtr> channelData) {
    // Validate channel data first (before initialization)
    if (channelData.size() == 0) {
        FL_LOG_PARLIO("PARLIO: No channels to transmit");
        return;
    }

    // Validate channel count is within bounds
    size_t channel_count = channelData.size();
    if (channel_count > 16) {
        FL_WARN("PARLIO: Too many channels (got " << channel_count
                                                  << ", max 16)");
        return;
    }

    // Validate channel count matches data_width constraints
    size_t required_width = selectDataWidth(channel_count);
    if (required_width != mState.data_width) {
        FL_WARN("PARLIO: Channel count "
                << channel_count << " requires data_width=" << required_width
                << " but this instance is data_width=" << mState.data_width);
        return;
    }

    // Store actual channel count (for dummy lane logic)
    mState.actual_channels = channel_count;
    mState.dummy_lanes = mState.data_width - channel_count;

    if (mState.dummy_lanes > 0) {
        FL_LOG_PARLIO("PARLIO: Using " << mState.dummy_lanes << " dummy lanes "
                                       << "(channels=" << channel_count
                                       << ", width=" << mState.data_width
                                       << ")");
    }

    // Extract and validate GPIO pins from channel data
    mState.pins.clear();
    for (size_t i = 0; i < channel_count; i++) {
        int pin = channelData[i]->getPin();

        // Validate pin using FastLED's pin validation system
        if (!isParlioPinValid(pin)) {
            FL_WARN("PARLIO: Invalid pin " << pin << " for channel " << i);
            FL_WARN("  This pin is either forbidden (SPI flash, strapping, etc.)");
            FL_WARN("  or not a valid output pin for this ESP32 variant.");
            FL_WARN("  See FASTLED_UNUSABLE_PIN_MASK in fastpin_esp32.h for details.");
            return;
        }

        mState.pins.push_back(pin);
        FL_LOG_PARLIO("PARLIO: Channel " << i << " using GPIO " << pin);
    }

    // Fill remaining pins with -1 for dummy lanes (unused)
    for (size_t i = channel_count; i < mState.data_width; i++) {
        mState.pins.push_back(-1);
    }

    // Extract timing configuration from the first channel
    // All channels in a batch must have the same timing, so we use the first channel's timing
    const ChipsetTimingConfig& timing = channelData[0]->getTiming();
    mState.timing_t1_ns = timing.t1_ns;
    mState.timing_t2_ns = timing.t2_ns;
    mState.timing_t3_ns = timing.t3_ns;

    // Ensure PARLIO peripheral is initialized
    initializeIfNeeded();

    // If initialization failed, abort
    if (!mInitialized || mState.tx_unit == nullptr) {
        FL_WARN("PARLIO: Cannot transmit - initialization failed");
        return;
    }

    // Check if we're already transmitting
    if (mState.transmitting) {
        FL_WARN("PARLIO: Transmission already in progress");
        return;
    }

    // Validate LED data and find maximum channel size
    size_t maxChannelSize = 0;
    bool hasData = false;

    for (size_t i = 0; i < channelData.size(); i++) {
        if (!channelData[i]) {
            FL_WARN("PARLIO: Null channel data at index " << i);
            return;
        }

        size_t dataSize = channelData[i]->getSize();

        if (dataSize > 0) {
            hasData = true;
            if (dataSize > maxChannelSize) {
                maxChannelSize = dataSize;
            }
        }

    }

    if (!hasData || maxChannelSize == 0) {
        FL_LOG_PARLIO("PARLIO: No LED data to transmit");
        return;
    }

    size_t numLeds = maxChannelSize / 3;
    FL_LOG_PARLIO("PARLIO: beginTransmission - " << channelData.size()
                                                 << " channels, " << numLeds
                                                 << " LEDs (streaming mode)");

    // Prepare single segmented scratch buffer for all N lanes
    // Layout: [lane0_data][lane1_data]...[laneN_data]
    // Each lane is maxChannelSize bytes (padded to same size)
    // This is regular heap (non-DMA) - only output waveform buffers need DMA
    // capability

    size_t totalBufferSize = channelData.size() * maxChannelSize;
    mState.scratch_padded_buffer.resize(totalBufferSize);
    mState.lane_stride = maxChannelSize;
    mState.num_lanes = channelData.size();

    FL_LOG_PARLIO("PARLIO: Prepared scratch buffer: "
                  << totalBufferSize << " bytes (" << channelData.size()
                  << " lanes × " << maxChannelSize << " bytes)");

    // Write all channels to segmented scratch buffer with stride-based layout
    for (size_t i = 0; i < channelData.size(); i++) {
        size_t dataSize = channelData[i]->getSize();
        uint8_t *laneDst =
            mState.scratch_padded_buffer.data() + (i * maxChannelSize);
        fl::span<uint8_t> dst(laneDst, maxChannelSize);

        // PARLIO FIX: Never use left-padding for multi-lane configurations
        // All lanes MUST have same number of LEDs, so right-pad with zeros if needed
        const auto &srcData = channelData[i]->getData();
        if (dataSize < maxChannelSize) {
            // Lane needs padding - use RIGHT-padding (zeros at end) instead of left-padding
            FL_LOG_PARLIO("PARLIO: Right-padding lane " << i << " from " << dataSize
                                                  << " to " << maxChannelSize
                                                  << " bytes");
            fl::memcpy(laneDst, srcData.data(), dataSize);  // Copy actual data first
            fl::memset(laneDst + dataSize, 0, maxChannelSize - dataSize);  // Zero-fill remainder
        } else {
            // No padding needed - direct copy
            fl::memcpy(laneDst, srcData.data(), maxChannelSize);
        }
    }

    // Initialize streaming state
    mState.total_leds = numLeds;
    mState.current_led = 0;
    mState.stream_complete = false;
    mState.error_occurred = false;
    mState.transmitting = true;

    // Transpose and queue first chunk (into buffer_a)
    mState.fill_buffer = mState.buffer_a.get();
    mState.active_buffer = nullptr; // No active transmission yet

    if (!transposeAndQueueNextChunk()) {
        FL_WARN("PARLIO: Failed to queue initial chunk");
        mState.transmitting = false;
        return;
    }

    FL_LOG_PARLIO("PARLIO: Initial chunk queued, streaming started");

    // If there are more chunks, queue second chunk (into buffer_b)
    if (mState.current_led < mState.total_leds) {
        if (!transposeAndQueueNextChunk()) {
            FL_WARN("PARLIO: Failed to queue second chunk");
            mState.transmitting = false;
            return;
        }
        FL_LOG_PARLIO("PARLIO: Second chunk queued");
    }

    // ISR will handle remaining chunks automatically
    FL_LOG_PARLIO("PARLIO: ISR callback count at transmission start: "
                  << g_parlio_isr_callback_count);
}

//=============================================================================
// Private Methods - Initialization
//=============================================================================

void ChannelEnginePARLIOImpl::initializeIfNeeded() {
    if (mInitialized) {
        return;
    }

    FL_LOG_PARLIO("PARLIO: Starting initialization (data_width=" << mState.data_width << ")");

    // Version compatibility check for ESP32-C6
    // PARLIO driver has a bug in IDF versions < 5.5 on ESP32-C6
#if defined(CONFIG_IDF_TARGET_ESP32C6)
#if ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(5, 5, 0)
    FL_WARN("PARLIO: ESP32-C6 requires ESP-IDF 5.5.0 or later (current version: "
            << ESP_IDF_VERSION_MAJOR << "." << ESP_IDF_VERSION_MINOR << "." << ESP_IDF_VERSION_PATCH
            << "). Earlier versions have a known bug in the PARLIO driver. "
            << "Initialization may fail or produce incorrect output.");
#endif
#endif

    // Hardware capability validation
    // Validate requested data width against chip's hardware capabilities
    // All current PARLIO chips (ESP32-P4, C6, H2, C5) support 16-bit mode,
    // but this check future-proofs against chips with limited PARLIO support
#if defined(SOC_PARLIO_TX_UNIT_MAX_DATA_WIDTH)
    if (mState.data_width > SOC_PARLIO_TX_UNIT_MAX_DATA_WIDTH) {
        FL_WARN("PARLIO: Requested data width " << mState.data_width
                << " bits exceeds hardware maximum " << SOC_PARLIO_TX_UNIT_MAX_DATA_WIDTH
                << " bits on this chip. "
                << "This chip does not support " << mState.data_width << "-bit mode. "
                << "Maximum supported: " << SOC_PARLIO_TX_UNIT_MAX_DATA_WIDTH << " bits. "
                << "Reduce channel count or use a chip with wider PARLIO support.");
        mInitialized = false;
        return;
    }
    FL_LOG_PARLIO("PARLIO: Hardware capability check passed (requested="
                  << mState.data_width << " bits, maximum="
                  << SOC_PARLIO_TX_UNIT_MAX_DATA_WIDTH << " bits)");
#else
    // Older ESP-IDF without SOC capability macros - perform basic validation
    // This fallback ensures compatibility with older IDF versions while still
    // providing some level of validation
    if (mState.data_width > 16) {
        FL_WARN("PARLIO: Requested data width " << mState.data_width
                << " bits exceeds assumed maximum (16 bits). "
                << "Hardware capability unknown - update ESP-IDF for proper detection. "
                << "Proceeding with caution...");
        // Don't fail - older IDF may still work, just warn
    }
#endif

    // Step 1: Calculate width-adaptive streaming chunk size
    mState.leds_per_chunk = calculateChunkSize(mState.data_width);
    FL_LOG_PARLIO("PARLIO: Chunk size=" << mState.leds_per_chunk << " LEDs (optimized for " << mState.data_width << "-bit width)");

    // Step 1: Generate precomputed waveforms using timing from chipset configuration
    // Timing parameters are extracted from the first channel in beginTransmission()
    // and stored in mState.timing_t1_ns, mState.timing_t2_ns, mState.timing_t3_ns
    //
    // For WS2812B-V5 (typical values):
    // - T1: 225ns (T0H - initial HIGH time)
    // - T2: 355ns (T1H-T0H - additional HIGH time for bit 1)
    // - T3: 645ns (T0L - LOW tail duration)
    //
    // Resulting waveforms (with 8.0 MHz = 125ns per pulse):
    // - Bit 0: T1 HIGH + T3 LOW = 225ns H + 645ns L ≈ 2 + 5 pulses
    // - Bit 1: (T1+T2) HIGH + T3 LOW = 580ns H + 645ns L ≈ 5 + 5 pulses
    FL_LOG_PARLIO("PARLIO: Using chipset timing - T1=" << mState.timing_t1_ns
                  << "ns, T2=" << mState.timing_t2_ns
                  << "ns, T3=" << mState.timing_t3_ns << "ns");

    size_t bit0_size = fl::generateBit0Waveform(
        PARLIO_CLOCK_FREQ_HZ, mState.timing_t1_ns, mState.timing_t2_ns, mState.timing_t3_ns,
        mState.bit0_waveform.data(), mState.bit0_waveform.size());
    size_t bit1_size = fl::generateBit1Waveform(
        PARLIO_CLOCK_FREQ_HZ, mState.timing_t1_ns, mState.timing_t2_ns, mState.timing_t3_ns,
        mState.bit1_waveform.data(), mState.bit1_waveform.size());

    if (bit0_size == 0 || bit1_size == 0 || bit0_size != bit1_size) {
        FL_WARN("PARLIO: Failed to generate waveforms (bit0="
                << bit0_size << ", bit1=" << bit1_size << ")");
        mInitialized = false;
        return;
    }

    mState.pulses_per_bit = bit0_size;
    FL_LOG_PARLIO("PARLIO: Generated waveforms: " << mState.pulses_per_bit
                                                  << " pulses per bit");

    // ITERATION 15: Detailed waveform timing analysis
    FL_LOG_PARLIO("PARLIO: Waveform Timing Analysis (DEBUG):");
    FL_LOG_PARLIO("  Clock: " << PARLIO_CLOCK_FREQ_HZ << " Hz (" << (1000000000UL / PARLIO_CLOCK_FREQ_HZ) << "ns per tick)");
    FL_LOG_PARLIO("  Timing: T1=" << mState.timing_t1_ns << "ns, T2=" << mState.timing_t2_ns << "ns, T3=" << mState.timing_t3_ns << "ns");
    FL_LOG_PARLIO("  Total expected: " << (mState.timing_t1_ns + mState.timing_t2_ns + mState.timing_t3_ns) << "ns per bit");
    FL_LOG_PARLIO("  Bit 0: " << bit0_size << " pulses");
    FL_LOG_PARLIO("  Bit 1: " << bit1_size << " pulses");

    // Count HIGH/LOW pulses for bit 0
    size_t bit0_high_count = 0;
    for (size_t i = 0; i < bit0_size; i++) {
        if (mState.bit0_waveform[i] == 0xFF) bit0_high_count++;
    }
    size_t bit0_low_count = bit0_size - bit0_high_count;

    // Count HIGH/LOW pulses for bit 1
    size_t bit1_high_count = 0;
    for (size_t i = 0; i < bit1_size; i++) {
        if (mState.bit1_waveform[i] == 0xFF) bit1_high_count++;
    }
    size_t bit1_low_count = bit1_size - bit1_high_count;

    uint32_t resolution_ns = 1000000000UL / PARLIO_CLOCK_FREQ_HZ;
    FL_LOG_PARLIO("  Bit 0 breakdown: " << bit0_high_count << " HIGH (" << (bit0_high_count * resolution_ns) << "ns), "
            << bit0_low_count << " LOW (" << (bit0_low_count * resolution_ns) << "ns) = "
            << (bit0_size * resolution_ns) << "ns total");
    FL_LOG_PARLIO("  Bit 1 breakdown: " << bit1_high_count << " HIGH (" << (bit1_high_count * resolution_ns) << "ns), "
            << bit1_low_count << " LOW (" << (bit1_low_count * resolution_ns) << "ns) = "
            << (bit1_size * resolution_ns) << "ns total");

    // Calculate expected vs actual
    uint32_t expected_total_ns = mState.timing_t1_ns + mState.timing_t2_ns + mState.timing_t3_ns;
    uint32_t actual_total_ns = bit0_size * resolution_ns;
    int32_t timing_error_ns = (int32_t)actual_total_ns - (int32_t)expected_total_ns;
    int32_t timing_error_percent = (timing_error_ns * 100) / expected_total_ns;

    // Only report as ERROR if timing deviation exceeds 50ns threshold
    constexpr int32_t TIMING_ERROR_THRESHOLD_NS = 50;
    int32_t abs_timing_error_ns = timing_error_ns < 0 ? -timing_error_ns : timing_error_ns;

    if (abs_timing_error_ns > TIMING_ERROR_THRESHOLD_NS) {
        FL_ERROR("  Timing error: " << timing_error_ns << "ns (" << timing_error_percent << "% deviation from spec)");
    } else {
        FL_WARN("  Timing error: " << timing_error_ns << "ns (" << timing_error_percent << "% deviation from spec)");
    }

    // Step 2: Calculate width-adaptive streaming chunk size (MUST be after
    // waveform generation) Now that we know pulses_per_bit, calculate optimal
    // chunk size for ~2KB buffers Formula matches transposeAndQueueNextChunk()
    // logic: For data_width <= 8: writes 1 byte per pulse tick For data_width ==
    // 16: writes 2 bytes per pulse tick
    size_t bytes_per_led_calc;
    if (mState.data_width <= 8) {
        // One output byte per pulse tick
        bytes_per_led_calc = 3 * 8 * mState.pulses_per_bit;
    } else {
        // Two output bytes per pulse tick for 16-bit width
        bytes_per_led_calc = 3 * 8 * mState.pulses_per_bit * 2;
    }
    constexpr size_t target_buffer_size = 2048; // 2KB per buffer
    mState.leds_per_chunk = target_buffer_size / bytes_per_led_calc;

    // Clamp to reasonable range
    if (mState.leds_per_chunk < 10)
        mState.leds_per_chunk = 10;
    if (mState.leds_per_chunk > 500)
        mState.leds_per_chunk = 500;

    FL_LOG_PARLIO("PARLIO: Chunk size=" << mState.leds_per_chunk
                                        << " LEDs (optimized for "
                                        << mState.data_width << "-bit width, "
                                        << mState.pulses_per_bit
                                        << " pulses/bit = " << bytes_per_led_calc
                                        << " bytes/LED)");

    // Step 3: Validate GPIO pins were provided by beginTransmission()
    // Pins are extracted from ChannelData and validated before initialization
    if (mState.pins.size() != mState.data_width) {
        FL_WARN("PARLIO: Pin configuration error - expected "
                << mState.data_width << " pins, got " << mState.pins.size());
        FL_WARN("  Pins must be provided via FastLED.addLeds<WS2812, PIN>() API");
        mInitialized = false;
        return;
    }

    FL_LOG_PARLIO("PARLIO: Using " << mState.data_width << " GPIO pins from user configuration");

    // Step 4: Configure PARLIO TX unit
    parlio_tx_unit_config_t config = {};
    config.clk_src = PARLIO_CLK_SRC_DEFAULT;
    config.clk_in_gpio_num =
        static_cast<gpio_num_t>(-1); // Use internal clock, not external
    config.output_clk_freq_hz = PARLIO_CLOCK_FREQ_HZ;
    config.data_width = mState.data_width; // Runtime parameter
    config.trans_queue_depth =
        8; // PHASE 1: Increased from 4 to 8 for minimum ISR gap (per PARLIO_INFO.md)
    config.max_transfer_size =
        4096; // ITERATION 2: Must accommodate buffer size (2016 bytes)
    config.bit_pack_order =
        PARLIO_BIT_PACK_ORDER_LSB; // Match bit-packing logic (bit 0 = tick 0)
    config.sample_edge =
        PARLIO_SAMPLE_EDGE_POS; // ITERATION 1: Match ESP-IDF example (was NEG)

    // Assign GPIO pins for data_width lanes
    for (size_t i = 0; i < mState.data_width; i++) {
        config.data_gpio_nums[i] = static_cast<gpio_num_t>(mState.pins[i]);
    }
    // Mark unused pins as invalid
    for (size_t i = mState.data_width; i < 16; i++) {
        config.data_gpio_nums[i] = static_cast<gpio_num_t>(-1);
    }

    config.clk_out_gpio_num = static_cast<gpio_num_t>(-1);
    config.valid_gpio_num = static_cast<gpio_num_t>(-1);

    // Step 5: Create TX unit
    esp_err_t err = parlio_new_tx_unit(&config, &mState.tx_unit);
    if (err != ESP_OK) {
        FL_WARN("PARLIO: Failed to create TX unit: " << err);
        mInitialized = false;
        return;
    }

    FL_LOG_PARLIO("PARLIO: TX unit created");

    // Step 6: Enable TX unit
    err = parlio_tx_unit_enable(mState.tx_unit);
    if (err != ESP_OK) {
        FL_WARN("PARLIO: Failed to enable TX unit: " << err);
        parlio_del_tx_unit(mState.tx_unit);
        mState.tx_unit = nullptr;
        mInitialized = false;
        return;
    }

    FL_LOG_PARLIO("PARLIO: TX unit enabled");

    // Step 7: Register ISR callback for streaming
    parlio_tx_event_callbacks_t callbacks = {};
    callbacks.on_trans_done =
        reinterpret_cast<parlio_tx_done_callback_t>(txDoneCallback);

    err = parlio_tx_unit_register_event_callbacks(mState.tx_unit, &callbacks,
                                                  this);
    if (err != ESP_OK) {
        FL_WARN("PARLIO: Failed to register callbacks: " << err);
        // Clean up: disable then delete (unit was enabled at line 574)
        esp_err_t disable_err = parlio_tx_unit_disable(mState.tx_unit);
        if (disable_err != ESP_OK) {
            FL_WARN("PARLIO: Failed to disable TX unit during cleanup: "
                    << disable_err);
        }
        esp_err_t del_err = parlio_del_tx_unit(mState.tx_unit);
        if (del_err != ESP_OK) {
            FL_WARN(
                "PARLIO: Failed to delete TX unit during cleanup: " << del_err);
        }
        mState.tx_unit = nullptr;
        mInitialized = false;
        return;
    }

    FL_LOG_PARLIO("PARLIO: ISR callbacks registered");

    // Step 8: Allocate double buffers for ping-pong streaming
    // CRITICAL REQUIREMENTS:
    // 1. MUST be DMA-capable memory (MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL)
    //    Required for PARLIO/GDMA hardware to access these buffers
    // 2. MUST be heap-allocated (NOT stack) - accessed by DMA hardware
    // asynchronously
    // 3. Buffer size depends on data_width and actual pulses_per_bit
    // (calculated at runtime)

    // ITERATION 2: CRITICAL FIX - Match transpose logic exactly!
    // The transposeAndQueueNextChunk() function writes:
    // - For data_width <= 8: 1 output byte per pulse tick
    //   → bytes_per_led = 3 colors × 8 bits × pulses_per_bit
    // - For data_width == 16: 2 output bytes per pulse tick
    //   → bytes_per_led = 3 colors × 8 bits × pulses_per_bit × 2
    //
    // Previous bug: divided by 8 assuming bit-packing, but transpose writes
    // full bytes per pulse (inefficient for data_width=1, but that's the logic)
    size_t bytes_per_led;
    if (mState.data_width <= 8) {
        // One output byte per pulse tick (see transposeAndQueueNextChunk line 360)
        bytes_per_led = 3 * 8 * mState.pulses_per_bit;
    } else {
        // Two output bytes per pulse tick for 16-bit width (line 378-380)
        bytes_per_led = 3 * 8 * mState.pulses_per_bit * 2;
    }
    size_t bufferSize = mState.leds_per_chunk * bytes_per_led;

    FL_LOG_PARLIO("PARLIO: Buffer size calculation:");
    FL_LOG_PARLIO("  pulses_per_bit: " << mState.pulses_per_bit);
    FL_LOG_PARLIO("  bytes_per_led: " << bytes_per_led);
    FL_LOG_PARLIO("  leds_per_chunk: " << mState.leds_per_chunk);
    FL_LOG_PARLIO("  buffer_size: " << bufferSize << " bytes");
    FL_LOG_PARLIO("PARLIO: Requesting 2 buffers × "
            << bufferSize << " bytes = " << (bufferSize * 2) << " bytes total");
    FL_LOG_PARLIO("PARLIO: Allocating DMA buffers from heap");

    // Allocate DMA-capable buffers from heap using fl::unique_ptr with custom
    // deleter
    mState.buffer_a.reset(static_cast<uint8_t *>(
        heap_caps_malloc(bufferSize, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL)));

    mState.buffer_b.reset(static_cast<uint8_t *>(
        heap_caps_malloc(bufferSize, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL)));

    // Check allocation success
    if (!mState.buffer_a || !mState.buffer_b) {
        FL_WARN("PARLIO: Failed to allocate DMA buffers (requested "
                << bufferSize << " bytes each)");

        // Clean up partial allocation (fl::unique_ptr will auto-free on reset)
        mState.buffer_a.reset();
        mState.buffer_b.reset();

        // Clean up TX unit
        esp_err_t disable_err = parlio_tx_unit_disable(mState.tx_unit);
        if (disable_err != ESP_OK) {
            FL_WARN("PARLIO: Failed to disable TX unit during buffer cleanup: "
                    << disable_err);
        }
        esp_err_t del_err = parlio_del_tx_unit(mState.tx_unit);
        if (del_err != ESP_OK) {
            FL_WARN("PARLIO: Failed to delete TX unit during buffer cleanup: "
                    << del_err);
        }
        mState.tx_unit = nullptr;
        mInitialized = false;
        return;
    }

    mState.buffer_size = bufferSize;

    FL_LOG_PARLIO("PARLIO: DMA-capable double buffers allocated (2 × "
                  << bufferSize << " = " << (bufferSize * 2)
                  << " bytes total)");

    // Step 9: Allocate waveform expansion buffer (avoids heap_caps_malloc in
    // IRAM) Calculate buffer size: 16 lanes (max) × 8 bits × 16 pulses (max) =
    // 2048 bytes CRITICAL REQUIREMENTS:
    // 1. MUST be heap-allocated (NOT stack) - used in ISR context and may be
    // large (up to 2KB)
    // 2. Does NOT need to be DMA-capable - only used temporarily for waveform
    // expansion
    //    in the ISR, not directly accessed by DMA hardware
    // 3. buffer_a and buffer_b (allocated above) MUST be DMA-capable for
    // PARLIO/GDMA hardware
    // 4. This buffer is managed by mState and freed in the destructor - never
    // free the
    //    temporary pointer (laneWaveforms) that points to this buffer in
    //    transposeAndQueueNextChunk()
    const size_t max_pulses_per_bit = 16;
    const size_t bytes_per_lane = 8 * max_pulses_per_bit; // 128 bytes per lane
    const size_t waveform_buffer_size = mState.data_width * bytes_per_lane;

    mState.waveform_expansion_buffer.reset(static_cast<uint8_t *>(
        heap_caps_malloc(waveform_buffer_size, MALLOC_CAP_INTERNAL)));

    if (!mState.waveform_expansion_buffer) {
        FL_WARN("PARLIO: Failed to allocate waveform expansion buffer ("
                << waveform_buffer_size << " bytes)");

        // Clean up allocated buffers (fl::unique_ptr will auto-free on reset)
        mState.buffer_a.reset();
        mState.buffer_b.reset();

        // Clean up TX unit
        esp_err_t disable_err = parlio_tx_unit_disable(mState.tx_unit);
        if (disable_err != ESP_OK) {
            FL_WARN("PARLIO: Failed to disable TX unit during waveform buffer "
                    "cleanup: "
                    << disable_err);
        }
        esp_err_t del_err = parlio_del_tx_unit(mState.tx_unit);
        if (del_err != ESP_OK) {
            FL_WARN("PARLIO: Failed to delete TX unit during waveform buffer "
                    "cleanup: "
                    << del_err);
        }
        mState.tx_unit = nullptr;
        mInitialized = false;
        return;
    }

    mState.waveform_expansion_buffer_size = waveform_buffer_size;
    FL_LOG_PARLIO("PARLIO: Waveform expansion buffer allocated ("
                  << waveform_buffer_size << " bytes)");

    // Initialize streaming state
    mState.transmitting = false;
    mState.stream_complete = false;
    mState.error_occurred = false;
    mState.current_led = 0;
    mState.total_leds = 0;

    mInitialized = true;
    FL_LOG_PARLIO("PARLIO: Initialization complete (streaming mode active)");
}

//=============================================================================
// Polymorphic Wrapper Class Implementation
//=============================================================================

ChannelEnginePARLIO::ChannelEnginePARLIO()
    : mEngine(), mCurrentDataWidth(0) {
    FL_LOG_PARLIO("PARLIO: Engine created (lazy initialization - no hardware configured yet)");
}

ChannelEnginePARLIO::~ChannelEnginePARLIO() {
    FL_LOG_PARLIO("PARLIO: Destroying engine");
    // fl::unique_ptr automatically deletes mEngine when it goes out of scope (RAII)
    mCurrentDataWidth = 0;
    FL_LOG_PARLIO("PARLIO: Engine destroyed");
}

void ChannelEnginePARLIO::enqueue(ChannelDataPtr channelData) {
    if (channelData) {
        mEnqueuedChannels.push_back(channelData);
    }
}

void ChannelEnginePARLIO::show() {
    if (!mEnqueuedChannels.empty()) {
        // Move enqueued channels to transmitting channels
        mTransmittingChannels = fl::move(mEnqueuedChannels);
        mEnqueuedChannels.clear();

        // Begin transmission (selects engine and delegates)
        beginTransmission(fl::span<const ChannelDataPtr>(
            mTransmittingChannels.data(), mTransmittingChannels.size()));
    }
}

IChannelEngine::EngineState ChannelEnginePARLIO::poll() {
    // Poll the engine if initialized
    if (mEngine) {
        EngineState state = mEngine->poll();

        // Clear transmitting channels when READY
        if (state == EngineState::READY && !mTransmittingChannels.empty()) {
            mTransmittingChannels.clear();
        }

        return state;
    }

    // No engine initialized = ready state (lazy initialization)
    return EngineState::READY;
}

void ChannelEnginePARLIO::beginTransmission(
    fl::span<const ChannelDataPtr> channelData) {
    // Validate channel data
    if (channelData.size() == 0) {
        FL_LOG_PARLIO("PARLIO: No channels to transmit");
        return;
    }

    size_t channel_count = channelData.size();

    // Validate channel count is within bounds
    if (channel_count > 16) {
        FL_WARN("PARLIO: Too many channels (got " << channel_count
                                                  << ", max 16)");
        return;
    }

    // Determine optimal data width for this transmission
    size_t required_width = selectDataWidth(channel_count);
    if (required_width == 0) {
        FL_WARN("PARLIO: Invalid channel count " << channel_count);
        return;
    }

    // Check if we need to create or reconfigure the engine
    bool need_reconfigure = false;

    if (!mEngine) {
        // First initialization - create engine with optimal width
        FL_LOG_PARLIO("PARLIO: Lazy initialization - creating " << required_width
                      << "-bit engine for " << channel_count << " channel(s)");
        mEngine = fl::make_unique<ChannelEnginePARLIOImpl>(required_width);
        mCurrentDataWidth = required_width;
    } else if (mCurrentDataWidth != required_width) {
        // Width changed - need to reconfigure
        FL_LOG_PARLIO("PARLIO: Channel count changed - reconfiguring from "
                      << mCurrentDataWidth << "-bit to " << required_width
                      << "-bit (channels: " << channel_count << ")");

        // Reset unique_ptr - automatically deletes old engine, creates new one
        mEngine.reset(new ChannelEnginePARLIOImpl(required_width));
        mCurrentDataWidth = required_width;
        need_reconfigure = true;
    }

    // Log configuration info
    if (!need_reconfigure && mEngine) {
        FL_LOG_PARLIO("PARLIO: Reusing " << mCurrentDataWidth << "-bit engine for "
                      << channel_count << " channel(s)");
    }

    // Delegate transmission to the single engine
    for (const auto &channel : channelData) {
        mEngine->enqueue(channel);
    }

    mEngine->show();
}

//=============================================================================
// Factory Function Implementation
//=============================================================================

fl::shared_ptr<IChannelEngine> createParlioEngine() {
    FL_LOG_PARLIO("PARLIO: Creating engine with lazy initialization");
    return fl::make_shared<ChannelEnginePARLIO>();
}

} // namespace fl

#endif // FASTLED_ESP32_HAS_PARLIO
#endif // ESP32
