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
FL_EXTERN_C_END

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
            const size_t pulsesPerByte =
                8 * mState.pulses_per_bit; // Should be 32

            if (mState.data_width <= 8) {
                // Pack into single bytes (1, 2, 4, or 8-bit widths)
                for (size_t tick = 0; tick < pulsesPerByte; tick++) {
                    uint8_t outputByte = 0;

                    // Extract one pulse from each lane and pack into bits
                    for (size_t lane = 0; lane < mState.data_width; lane++) {
                        uint8_t *laneWaveform =
                            laneWaveforms + (lane * bytes_per_lane);
                        uint8_t pulse = laneWaveform[tick];
                        uint8_t bit =
                            (pulse != 0) ? 1 : 0; // Convert 0xFF→1, 0x00→0
                        outputByte |= (bit << (mState.data_width - 1 - lane));
                    }

                    nextBuffer[outputIdx++] = outputByte;
                }
            } else if (mState.data_width == 16) {
                // Pack into 16-bit words (two bytes per tick)
                for (size_t tick = 0; tick < pulsesPerByte; tick++) {
                    uint16_t outputWord = 0;

                    // Extract one pulse from each lane and pack into bits
                    for (size_t lane = 0; lane < 16; lane++) {
                        uint8_t *laneWaveform =
                            laneWaveforms + (lane * bytes_per_lane);
                        uint8_t pulse = laneWaveform[tick];
                        uint8_t bit =
                            (pulse != 0) ? 1 : 0; // Convert 0xFF→1, 0x00→0
                        outputWord |= (bit << (15 - lane));
                    }

                    // Write as two bytes (high byte first, then low byte)
                    nextBuffer[outputIdx++] =
                        (outputWord >> 8) & 0xFF;                // High byte
                    nextBuffer[outputIdx++] = outputWord & 0xFF; // Low byte
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

        if (dataSize > 0 && dataSize % 3 != 0) {
            FL_WARN("PARLIO: Channel " << i << " has " << dataSize
                                       << " bytes (not a multiple of 3)");
            return;
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

        if (dataSize < maxChannelSize) {
            // Lane needs padding
            FL_LOG_PARLIO("PARLIO: Padding lane " << i << " from " << dataSize
                                                  << " to " << maxChannelSize
                                                  << " bytes");
            channelData[i]->writeWithPadding(dst);
        } else {
            // No padding needed - direct copy
            const auto &srcData = channelData[i]->getData();
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

    // Step 1: Calculate width-adaptive streaming chunk size
    mState.leds_per_chunk = calculateChunkSize(mState.data_width);
    FL_LOG_PARLIO("PARLIO: Chunk size=" << mState.leds_per_chunk << " LEDs (optimized for " << mState.data_width << "-bit width)");

    // Step 1: Generate precomputed waveforms using generic waveform generator
    // WS2812 timing parameters (nanoseconds) for 10-tick encoding at 8.0 MHz:
    // - T1: 375ns (initial HIGH time → 3 pulses at 125ns resolution)
    // - T2: 500ns (additional HIGH time for bit 1 → 4 pulses at 125ns
    // resolution)
    // - T3: 375ns (LOW tail duration → 3 pulses at 125ns resolution)
    //
    // Resulting waveforms:
    // - Bit 0: 3 pulses HIGH + 7 pulses LOW = 375ns H + 875ns L = 1250ns total
    // - Bit 1: 7 pulses HIGH + 3 pulses LOW = 875ns H + 375ns L = 1250ns total
    //
    // WS2812 specification compliance (8.0 MHz clock with standard timing):
    // - T0H: 375ns (spec: 400±150ns = 250-550ns) ✓
    // - T0L: 875ns (spec: 850±150ns = 700-1000ns) ✓
    // - T1H: 875ns (spec: 800±150ns = 650-950ns) ✓
    // - T1L: 375ns (spec: 450±150ns = 300-600ns) ✓
    // WS2812B timing adapted for 8MHz clock
    // With 8MHz (125ns/pulse), total = 10 pulses = 1.25μs per bit (standard
    // timing) Bit 0: t1=375ns (3 pulses HIGH), t2=500ns, t3=375ns (7 pulses LOW
    // total) Bit 1: t1=375ns, t2=500ns (7 pulses HIGH total), t3=375ns (3
    // pulses LOW) This matches standard WS2812 timing requirements
    size_t bit0_size = fl::generateBit0Waveform(
        PARLIO_CLOCK_FREQ_HZ, 375, 500, 375, mState.bit0_waveform.data(),
        mState.bit0_waveform.size());
    size_t bit1_size = fl::generateBit1Waveform(
        PARLIO_CLOCK_FREQ_HZ, 375, 500, 375, mState.bit1_waveform.data(),
        mState.bit1_waveform.size());

    if (bit0_size == 0 || bit1_size == 0 || bit0_size != bit1_size) {
        FL_WARN("PARLIO: Failed to generate waveforms (bit0="
                << bit0_size << ", bit1=" << bit1_size << ")");
        mInitialized = false;
        return;
    }

    mState.pulses_per_bit = bit0_size;
    FL_LOG_PARLIO("PARLIO: Generated waveforms: " << mState.pulses_per_bit
                                                  << " pulses per bit");

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
        4; // ITERATION 2: Reduced from 32 to avoid stack overflow
    config.max_transfer_size =
        4096; // ITERATION 2: Must accommodate buffer size (2016 bytes)
    config.bit_pack_order =
        PARLIO_BIT_PACK_ORDER_MSB; // WS2812 expects MSB-first
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
    // ITERATION 4: Heap check BEFORE parlio_new_tx_unit()
    FL_WARN("ITERATION 4: Heap check BEFORE parlio_new_tx_unit()");
    if (!heap_caps_check_integrity_all(true)) {
        FL_WARN("❌ HEAP CORRUPTION DETECTED BEFORE parlio_new_tx_unit()!");
        mInitialized = false;
        return;
    }
    FL_WARN("✓ Heap OK before parlio_new_tx_unit()");

    esp_err_t err = parlio_new_tx_unit(&config, &mState.tx_unit);
    if (err != ESP_OK) {
        FL_WARN("PARLIO: Failed to create TX unit: " << err);
        mInitialized = false;
        return;
    }

    FL_LOG_PARLIO("PARLIO: TX unit created");

    // ITERATION 4: Heap check AFTER parlio_new_tx_unit()
    FL_WARN("ITERATION 4: Heap check AFTER parlio_new_tx_unit()");
    if (!heap_caps_check_integrity_all(true)) {
        FL_WARN("❌ HEAP CORRUPTION DETECTED AFTER parlio_new_tx_unit()!");
        mInitialized = false;
        return;
    }
    FL_WARN("✓ Heap OK after parlio_new_tx_unit()");

    // Step 6: Enable TX unit
    FL_WARN("ITERATION 4: Heap check BEFORE parlio_tx_unit_enable()");
    if (!heap_caps_check_integrity_all(true)) {
        FL_WARN("❌ HEAP CORRUPTION DETECTED BEFORE parlio_tx_unit_enable()!");
        mInitialized = false;
        return;
    }
    FL_WARN("✓ Heap OK before parlio_tx_unit_enable()");

    err = parlio_tx_unit_enable(mState.tx_unit);
    if (err != ESP_OK) {
        FL_WARN("PARLIO: Failed to enable TX unit: " << err);
        parlio_del_tx_unit(mState.tx_unit);
        mState.tx_unit = nullptr;
        mInitialized = false;
        return;
    }

    FL_LOG_PARLIO("PARLIO: TX unit enabled");

    // ITERATION 4: Heap check AFTER parlio_tx_unit_enable()
    FL_WARN("ITERATION 4: Heap check AFTER parlio_tx_unit_enable()");
    if (!heap_caps_check_integrity_all(true)) {
        FL_WARN("❌ HEAP CORRUPTION DETECTED AFTER parlio_tx_unit_enable()!");
        mInitialized = false;
        return;
    }
    FL_WARN("✓ Heap OK after parlio_tx_unit_enable()");

    // Step 7: Register ISR callback for streaming
    FL_WARN("ITERATION 4: Heap check BEFORE "
            "parlio_tx_unit_register_event_callbacks()");
    if (!heap_caps_check_integrity_all(true)) {
        FL_WARN("❌ HEAP CORRUPTION DETECTED BEFORE "
                "parlio_tx_unit_register_event_callbacks()!");
        mInitialized = false;
        return;
    }
    FL_WARN("✓ Heap OK before parlio_tx_unit_register_event_callbacks()");

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

    // ITERATION 4: Heap check AFTER parlio_tx_unit_register_event_callbacks()
    FL_WARN("ITERATION 4: Heap check AFTER "
            "parlio_tx_unit_register_event_callbacks()");
    if (!heap_caps_check_integrity_all(true)) {
        FL_WARN("❌ HEAP CORRUPTION DETECTED AFTER "
                "parlio_tx_unit_register_event_callbacks()!");
        mInitialized = false;
        return;
    }
    FL_WARN("✓ Heap OK after parlio_tx_unit_register_event_callbacks()");

    // Step 8: Allocate double buffers for ping-pong streaming
    // CRITICAL REQUIREMENTS:
    // 1. MUST be DMA-capable memory (MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL)
    //    Required for PARLIO/GDMA hardware to access these buffers
    // 2. MUST be heap-allocated (NOT stack) - accessed by DMA hardware
    // asynchronously
    // 3. Buffer size depends on data_width and actual pulses_per_bit
    // (calculated at runtime)

    // Diagnostic: Check DMA memory availability BEFORE allocation
    size_t dma_free =
        heap_caps_get_free_size(MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
    size_t dma_largest =
        heap_caps_get_largest_free_block(MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
    size_t total_free = heap_caps_get_free_size(MALLOC_CAP_8BIT);
    size_t total_largest = heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);

    FL_WARN("PARLIO: Memory diagnostics BEFORE buffer allocation:");
    FL_WARN("  DMA-capable: " << dma_free
                              << " bytes free, largest block: " << dma_largest);
    FL_WARN("  Total heap: " << total_free << " bytes free, largest block: "
                             << total_largest);

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

    FL_WARN("PARLIO: Buffer size calculation:");
    FL_WARN("  pulses_per_bit: " << mState.pulses_per_bit);
    FL_WARN("  bytes_per_led: " << bytes_per_led);
    FL_WARN("  leds_per_chunk: " << mState.leds_per_chunk);
    FL_WARN("  buffer_size: " << bufferSize << " bytes");
    FL_WARN("PARLIO: Requesting 2 buffers × "
            << bufferSize << " bytes = " << (bufferSize * 2) << " bytes total");
    FL_WARN("PARLIO: Allocating DMA buffers from heap");

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
        FL_WARN("  DMA-capable memory: " << dma_free << " bytes available");

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

    FL_WARN("PARLIO: Successfully allocated DMA buffers");
    FL_WARN("  buffer_a: " << (void *)mState.buffer_a.get() << " ("
                           << bufferSize << " bytes)");
    FL_WARN("  buffer_b: " << (void *)mState.buffer_b.get() << " ("
                           << bufferSize << " bytes)");

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
    : mEngine1Bit(nullptr), mEngine2Bit(nullptr), mEngine4Bit(nullptr),
      mEngine8Bit(nullptr), mEngine16Bit(nullptr), mActiveEngine(nullptr) {
    FL_LOG_PARLIO(
        "PARLIO: Creating polymorphic engine (1/2/4/8/16-bit auto-select)");

    // Create all sub-engines at construction (max capability allocation)
    mEngine1Bit = new ChannelEnginePARLIOImpl(1);
    mEngine2Bit = new ChannelEnginePARLIOImpl(2);
    mEngine4Bit = new ChannelEnginePARLIOImpl(4);
    mEngine8Bit = new ChannelEnginePARLIOImpl(8);
    mEngine16Bit = new ChannelEnginePARLIOImpl(16);

    FL_LOG_PARLIO("PARLIO: Polymorphic engine ready (all 1/2/4/8/16-bit "
                  "sub-engines created)");
}

ChannelEnginePARLIO::~ChannelEnginePARLIO() {
    FL_LOG_PARLIO("PARLIO: Destroying polymorphic engine");

    // Delete all sub-engines
    if (mEngine1Bit) {
        delete mEngine1Bit;
        mEngine1Bit = nullptr;
    }

    if (mEngine2Bit) {
        delete mEngine2Bit;
        mEngine2Bit = nullptr;
    }

    if (mEngine4Bit) {
        delete mEngine4Bit;
        mEngine4Bit = nullptr;
    }

    if (mEngine8Bit) {
        delete mEngine8Bit;
        mEngine8Bit = nullptr;
    }

    if (mEngine16Bit) {
        delete mEngine16Bit;
        mEngine16Bit = nullptr;
    }

    mActiveEngine = nullptr;

    FL_LOG_PARLIO("PARLIO: Polymorphic engine destroyed");
}

void ChannelEnginePARLIO::enqueue(ChannelDataPtr channelData) {
    FL_DBG("ChannelEnginePARLIO::enqueue() - channel="
           << (void *)channelData.get());
    if (channelData) {
        mEnqueuedChannels.push_back(channelData);
        FL_DBG("ChannelEnginePARLIO::enqueue() - Added to queue (total="
               << mEnqueuedChannels.size() << ")");
    }
}

void ChannelEnginePARLIO::show() {
    FL_DBG("ChannelEnginePARLIO::show() - START (enqueued="
           << mEnqueuedChannels.size() << ")");

    if (!mEnqueuedChannels.empty()) {
        FL_DBG("ChannelEnginePARLIO::show() - Moving channels to transmitting "
               "list");
        // Move enqueued channels to transmitting channels
        mTransmittingChannels = fl::move(mEnqueuedChannels);
        mEnqueuedChannels.clear();

        FL_DBG("ChannelEnginePARLIO::show() - Calling beginTransmission with "
               << mTransmittingChannels.size() << " channels");
        // Begin transmission (selects engine and delegates)
        beginTransmission(fl::span<const ChannelDataPtr>(
            mTransmittingChannels.data(), mTransmittingChannels.size()));
        FL_DBG("ChannelEnginePARLIO::show() - beginTransmission returned");
    }

    FL_DBG("ChannelEnginePARLIO::show() - COMPLETE");
}

IChannelEngine::EngineState ChannelEnginePARLIO::poll() {
    // Poll the active engine if one is selected
    if (mActiveEngine) {
        FL_DBG("ChannelEnginePARLIO::poll() - Calling poll() on active "
               "sub-engine");
        EngineState state = mActiveEngine->poll();
        FL_DBG("ChannelEnginePARLIO::poll() - Sub-engine returned state="
               << (int)state);

        // Clear transmitting channels when READY
        if (state == EngineState::READY && !mTransmittingChannels.empty()) {
            FL_DBG(
                "ChannelEnginePARLIO::poll() - Clearing transmitting channels");
            mTransmittingChannels.clear();
        }

        FL_DBG("ChannelEnginePARLIO::poll() - COMPLETE (state=" << (int)state
                                                                << ")");
        return state;
    }

    // No active engine = ready state (no debug output to reduce noise when
    // engine is disabled)
    return EngineState::READY;
}

void ChannelEnginePARLIO::beginTransmission(
    fl::span<const ChannelDataPtr> channelData) {
    FL_DBG("ChannelEnginePARLIO::beginTransmission() - START (channels="
           << channelData.size() << ")");

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

    // Select optimal engine based on channel count using selectDataWidth()
    // helper
    FL_DBG(
        "ChannelEnginePARLIO::beginTransmission() - Selecting data width for "
        << channel_count << " channels");
    size_t optimal_width = selectDataWidth(channel_count);
    FL_DBG("ChannelEnginePARLIO::beginTransmission() - Optimal width="
           << optimal_width);

    switch (optimal_width) {
    case 1:
        mActiveEngine = mEngine1Bit;
        FL_LOG_PARLIO("PARLIO: Selected 1-bit engine for "
                      << channel_count
                      << " channel(s), ptr=" << (void *)mActiveEngine);
        break;
    case 2:
        mActiveEngine = mEngine2Bit;
        FL_LOG_PARLIO("PARLIO: Selected 2-bit engine for "
                      << channel_count
                      << " channel(s), ptr=" << (void *)mActiveEngine);
        break;
    case 4:
        mActiveEngine = mEngine4Bit;
        FL_LOG_PARLIO("PARLIO: Selected 4-bit engine for "
                      << channel_count
                      << " channel(s), ptr=" << (void *)mActiveEngine);
        break;
    case 8:
        mActiveEngine = mEngine8Bit;
        FL_LOG_PARLIO("PARLIO: Selected 8-bit engine for "
                      << channel_count
                      << " channel(s), ptr=" << (void *)mActiveEngine);
        break;
    case 16:
        mActiveEngine = mEngine16Bit;
        FL_LOG_PARLIO("PARLIO: Selected 16-bit engine for "
                      << channel_count
                      << " channel(s), ptr=" << (void *)mActiveEngine);
        break;
    default:
        FL_WARN("PARLIO: Invalid data width "
                << optimal_width << " for " << channel_count << " channel(s)");
        return;
    }

    FL_DBG("ChannelEnginePARLIO::beginTransmission() - Enqueueing "
           << channel_count << " channels to sub-engine");
    // Delegate transmission to selected engine
    // We need to enqueue the data and call show() on the sub-engine
    for (const auto &channel : channelData) {
        FL_DBG("ChannelEnginePARLIO::beginTransmission() - Enqueueing channel "
               "to sub-engine");
        mActiveEngine->enqueue(channel);
    }

    FL_DBG("ChannelEnginePARLIO::beginTransmission() - Calling show() on "
           "sub-engine");
    mActiveEngine->show();
    FL_DBG("ChannelEnginePARLIO::beginTransmission() - COMPLETE");
}

//=============================================================================
// Factory Function Implementation
//=============================================================================

fl::shared_ptr<IChannelEngine> createParlioEngine() {
    FL_LOG_PARLIO(
        "PARLIO: Creating polymorphic engine (1/2/4/8/16-bit auto-select)");
    return fl::make_shared<ChannelEnginePARLIO>();
}

} // namespace fl

#endif // FASTLED_ESP32_HAS_PARLIO
#endif // ESP32
