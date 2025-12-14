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
// Clock: 10.0 MHz (100ns per tick)
// Divides from PLL_F160M on ESP32-P4 (160/16) or PLL_F240M on ESP32-C6 (240/24)
// Each LED bit is encoded as 8 clock ticks (0.8μs total)
static constexpr uint32_t PARLIO_CLOCK_FREQ_HZ = 10000000; // 10.0 MHz


//=============================================================================
// Phase 4: Cross-Platform Memory Barriers
//=============================================================================
// Memory barriers ensure correct synchronization between ISR and main thread
// - ISR writes to volatile fields (stream_complete, transmitting, current_led)
// - Main thread reads volatile fields, then executes barrier before reading non-volatile fields
// - Barrier ensures all ISR writes are visible to main thread
//
// Platform-Specific Barriers:
// - Xtensa (ESP32, ESP32-S3): memw instruction (memory write barrier)
// - RISC-V (ESP32-C6, ESP32-C3, ESP32-H2): fence rw,rw instruction (full memory fence)
//
#if defined(__XTENSA__)
    // Xtensa architecture (ESP32, ESP32-S3)
    #define PARLIO_MEMORY_BARRIER() asm volatile("memw" ::: "memory")
#elif defined(__riscv)
    // RISC-V architecture (ESP32-C6, ESP32-C3, ESP32-H2, ESP32-P4)
    #define PARLIO_MEMORY_BARRIER() asm volatile("fence rw, rw" ::: "memory")
#else
    #error "Unsupported architecture for PARLIO memory barrier (expected __XTENSA__ or __riscv)"
#endif

//=============================================================================
// Singleton Instance Definition
//=============================================================================
// ParlioIsrContext singleton - defined in header, instance defined here
ParlioIsrContext* ParlioIsrContext::s_instance = nullptr;

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

/// @brief Calculate total DMA buffer size using official README.md formula
/// @param num_leds Number of LEDs per lane
/// @param data_width Number of parallel lanes (1, 2, 4, 8, or 16)
/// @return Total buffer size in bytes
/// @note Formula: buffer_size = (num_leds × 192 bits × data_width + 7) / 8 bytes
/// @note VALIDATION REQUIRED: Test this function with known values from README.md examples
static inline size_t calculateTotalBufferSize(size_t num_leds, size_t data_width) {
    // Official formula from README.md
    // Each LED = 192 bits (3 colors × 64-bit waveform each)
    // Result: 24 bytes per LED per lane, scaled by data_width for parallel transmission
    return (num_leds * 192 * data_width + 7) / 8;
}

/// @brief Calculate DMA chunk buffer size for ring buffer streaming
/// @param total_leds Total number of LEDs per lane to transmit
/// @param num_chunks Number of ring buffers to split transmission into
/// @param data_width Number of parallel lanes (1, 2, 4, 8, or 16)
/// @return Size of each chunk buffer in bytes (last chunk may be smaller)
/// @note VALIDATION REQUIRED: Verify chunk size fits within PARLIO hardware limits (65535 bytes max)
/// @note Last chunk will be smaller if total_leds not evenly divisible by num_chunks
static inline size_t calculateChunkBufferSize(size_t total_leds, size_t num_chunks, size_t data_width) {
    // Calculate LEDs per chunk (round up for buffer allocation)
    size_t leds_per_chunk = (total_leds + num_chunks - 1) / num_chunks;

    // Use official formula for chunk size
    return calculateTotalBufferSize(leds_per_chunk, data_width);
}

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
//     - Each LED byte → 8 ticks × 1 byte = 8 bytes (after waveform encoding)
//     - Each LED (RGB) → 24 bytes (after waveform encoding)
//
// BUFFER SIZE CALCULATION:
//   Scratch buffer: N LEDs × 3 colors × 8 strips bytes (per-strip layout)
//   DMA chunks: 100 LEDs × 3 × 8 bytes × 1 byte = 2400 bytes (per buffer)
//
// EXAMPLE (1000 LEDs, 8 strips):
//   Scratch buffer: 1000 × 3 × 8 = 24,000 bytes (per-strip layout)
//   DMA buffers: 2 × (100 × 3 × 8 × 1) = 4,800 bytes (ping-pong expanded
//   waveforms) Total: ~29 KB (waveform expansion happens in ISR, not stored)
//
// CHUNKING STRATEGY:
//   PARLIO has 65535 byte max transfer size. For large LED counts:
//   - Break transmission at LED boundaries (not in middle of RGB)
//   - Fixed chunk size: 100 LEDs (2400 bytes per chunk)
//   - Max ~2730 LEDs per chunk (65535 / 24 bytes per LED)
//   - Transmit chunks sequentially via ping-pong DMA buffering
//
// WAVEFORM GENERATION:
//   Uses generic waveform generator with WS2812 timing parameters:
//   - T1: 300ns (initial HIGH time)
//   - T2: 200ns (additional HIGH time for bit 1)
//   - T3: 500ns (LOW tail duration)
//   - Clock: 10.0 MHz (100ns per pulse)
//   - Result: 8 pulses per bit (bit0=3H+5L, bit1=5H+3L)
//
//-----------------------------------------------------------------------------

//=============================================================================
// Global Instance Tracker
//=============================================================================

// Phase 1: Global instance tracker for debug metrics access
// This is set by ChannelEnginePARLIOImpl constructor/destructor
static ChannelEnginePARLIOImpl* g_parlio_instance = nullptr;

//=============================================================================
// Constructors / Destructors - Implementation Class
//=============================================================================

ChannelEnginePARLIOImpl::ChannelEnginePARLIOImpl(size_t data_width)
    : mInitialized(false), mState(data_width) {
    // Phase 1: Set global instance pointer for debug metrics access
    g_parlio_instance = this;

    // Validate data width
    if (data_width != 1 && data_width != 2 && data_width != 4 &&
        data_width != 8 && data_width != 16) {
        FL_WARN("PARLIO: Invalid data_width="
                << data_width << " (must be 1, 2, 4, 8, or 16)");
        // Constructor will still complete, but initialization will fail
        return;
    }
}

ChannelEnginePARLIOImpl::~ChannelEnginePARLIOImpl() {
    // Phase 1: Clear global instance pointer for debug metrics
    if (g_parlio_instance == this) {
        g_parlio_instance = nullptr;
    }

    // Wait for any active transmissions to complete
    // Note: delayMicroseconds already yields to watchdog
    while (poll() == EngineState::BUSY) {
        fl::delayMicroseconds(100);
    }

    // Clean up PARLIO TX unit resources
    if (mState.tx_unit != nullptr) {
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
        }

    // DMA buffers and waveform expansion buffer are automatically freed
    // by fl::unique_ptr destructors (RAII)

    // Phase 4: Clean up IsrContext and clear singleton
    if (mState.mIsrContext) {
        ParlioIsrContext::setInstance(nullptr);  // Clear singleton reference
        delete mState.mIsrContext;
        mState.mIsrContext = nullptr;
    }

    // Clear state
    mState.pins.clear();
    mState.scratch_padded_buffer.clear();
    mState.buffer_size = 0;
    mState.num_lanes = 0;
    mState.lane_stride = 0;
    // Phase 4: Clear IsrContext state (if allocated)
    if (mState.mIsrContext) {
        mState.mIsrContext->transmitting = false;
        mState.mIsrContext->stream_complete = false;
    }
    mState.error_occurred = false;

    }

//=============================================================================
// Private Methods - ISR Streaming Support
//=============================================================================

// Phase 0: ISR submits next buffer when current buffer completes
// Per LOOP.md line 36: "parlio_tx_unit_transmit() must be called from the main thread (CPU)
// only once, after this it must be called from the on done callback ISR until the last buffer"
//
// ISR responsibilities:
//   1. Increment buffers_completed counter (signals CPU one buffer finished)
//   2. Check if more buffers need to be submitted
//   3. If yes: Submit next buffer from ring to hardware via parlio_tx_unit_transmit()
//   4. Update ring_read_ptr to indicate buffer consumed
//   5. Check for completion (all buffers transmitted)
//
// CPU responsibilities (in poll()):
//   - Pre-compute next DMA buffer when ring has space
//   - Update ring_write_ptr after pre-computation
//   - Detect stream_complete flag for transmission end
__attribute__((optimize("O3")))
bool FL_IRAM ChannelEnginePARLIOImpl::txDoneCallback(
    parlio_tx_unit_handle_t tx_unit, const void *edata, void *user_ctx) {
    (void)edata;

    auto *self = static_cast<ChannelEnginePARLIOImpl *>(user_ctx);
    if (!self || !self->mState.mIsrContext) {
        return false;
    }

    // Phase 0: Access ISR state via cache-aligned ParlioIsrContext struct
    ParlioIsrContext* ctx = self->mState.mIsrContext;

    // Step 1: Increment completion counter (signals CPU that one buffer finished)
    ctx->buffers_completed++;
    ctx->isr_count++;

    // Step 2: Check if we need to submit another buffer
    // We submit if: buffers_completed < buffers_total AND ring has data (read_ptr != write_ptr)
    if (ctx->buffers_completed < ctx->buffers_total) {
        // Check if next buffer is ready in ring
        // Ring is not empty when read_ptr != write_ptr
        volatile size_t read_ptr = ctx->ring_read_ptr;
        volatile size_t write_ptr = ctx->ring_write_ptr;

        if (read_ptr != write_ptr) {
            // Next buffer is ready - submit it to hardware
            size_t buffer_idx = read_ptr % ctx->ring_size;

            // Get buffer pointer and size from ring
            uint8_t* buffer_ptr = self->mState.ring_buffers[buffer_idx].get();
            size_t buffer_size = self->mState.ring_buffer_sizes[buffer_idx];

            if (buffer_ptr && buffer_size > 0) {
                // Submit buffer to hardware
                // Phase 1 - Iteration 2: Use consistent idle_value = 0x0000 (LOW) for all buffers
                // WS2812B protocol requires LOW state between data frames
                // Alternating idle_value creates spurious transitions between chunks
                // ITERATION 5 - Experiment 1-2: idle_value = HIGH FAILED (100% error rate)
                // ITERATION 5 - Experiment 3: Revert to idle_value = LOW, test with sample_edge = NEG
                parlio_transmit_config_t tx_config = {};
                tx_config.idle_value = 0x0000; // Keep pins LOW between chunks (reverted)
                // tx_config.idle_value = 0xFFFF; // ITERATION 5: HIGH failed (disabled)

                esp_err_t err = parlio_tx_unit_transmit(tx_unit, buffer_ptr,
                                                         buffer_size * 8, &tx_config);

                if (err == ESP_OK) {
                    // Successfully submitted - increment read pointer
                    ctx->ring_read_ptr++;
                } else {
                    // Submission failed - set error flag for CPU to detect
                    ctx->ring_error = true;
                }
            } else {
                // Invalid buffer - set error flag
                ctx->ring_error = true;
            }
        } else {
            // Ring underflow - CPU didn't generate buffer fast enough
            // This is an error condition (ring should always be ahead)
            ctx->ring_error = true;
        }
    } else {
        // All buffers have been submitted - mark transmission complete
        ctx->stream_complete = true;
        ctx->transmitting = false;
    }

    return false; // No high-priority task woken
}

bool FL_IRAM ChannelEnginePARLIOImpl::transposeAndQueueNextChunk() {
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
    fl::memset(nextBuffer, 0x00, mState.buffer_size);

    if (nextBuffer == nullptr) {
        return false;
    }

    // Use pre-allocated waveform expansion buffer
    const size_t max_pulses_per_bit = 16; // Conservative max
    const size_t bytes_per_lane = 8 * max_pulses_per_bit; // 128 bytes per lane
    const size_t waveform_buffer_size = mState.data_width * bytes_per_lane;

    if (!mState.waveform_expansion_buffer ||
        mState.waveform_expansion_buffer_size < waveform_buffer_size) {
        mState.error_occurred = true;
        return false;
    }

    uint8_t *laneWaveforms = mState.waveform_expansion_buffer.get();
    const uint8_t *bit0_wave = mState.bit0_waveform.data();
    const uint8_t *bit1_wave = mState.bit1_waveform.data();
    size_t waveform_size = mState.pulses_per_bit;

    // Zero waveform buffer to eliminate leftover data
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

                    // PHASE 9: Use optimized nibble lookup table for waveform expansion
                    size_t expanded = fl::expandByteToWaveformsOptimized(
                        byte, mState.nibble_lut, laneWaveform, bytes_per_lane);
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

                    // FIX C3-2: Check for buffer overflow BEFORE writing (prevent corruption)
                    if (outputIdx + 2 > mState.buffer_size) {
                        mState.error_occurred = true;
                        return false;
                    }

                    // Write as two bytes (low byte first for little-endian)
                    nextBuffer[outputIdx++] = outputWord & 0xFF;          // Low byte
                    nextBuffer[outputIdx++] = (outputWord >> 8) & 0xFF;  // High byte
                }
            }
        }
    }

    // FIX C3-2: Removed redundant buffer overflow check (now checked BEFORE each write)
    // Previously checked AFTER all writes completed, which was too late to prevent corruption

    // Queue transmission of this buffer
    // ITERATION 5 - Experiment 1-2: idle_value = HIGH FAILED (100% error rate)
    // ITERATION 5 - Experiment 3: Revert to idle_value = LOW, test with sample_edge = NEG
    parlio_transmit_config_t tx_config = {};
    tx_config.idle_value = 0x0000; // (reverted)
    // tx_config.idle_value = 0xFFFF; // ITERATION 5: HIGH failed (disabled)

    size_t chunkBits =
        outputIdx * 8; // PARLIO API requires payload size in bits

    esp_err_t err = parlio_tx_unit_transmit(mState.tx_unit, nextBuffer,
                                            chunkBits, &tx_config);

    if (err != ESP_OK) {
        mState.error_occurred = true;
        // Phase 0: Record error code
        // CRITICAL: laneWaveforms points to mState.waveform_expansion_buffer
        // (pre-allocated heap buffer managed by this class). DO NOT call
        // heap_caps_free(laneWaveforms)! The buffer is owned by mState and will
        // be freed in the destructor.
        return false;
    }

    // Update state
    mState.current_led += ledsThisChunk;

    // Phase 0: Track chunks queued and bytes transmitted
    size_t bytes_this_chunk = ledsThisChunk * 3 * mState.num_lanes;

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
// Phase 0: Ring Buffer Generation (CPU Thread)
//=============================================================================

bool ChannelEnginePARLIOImpl::generateRingBuffer(size_t ring_index, size_t start_led, size_t led_count) {
    // Validate parameters
    if (ring_index >= PARLIO_RING_BUFFER_COUNT) {
        FL_WARN("PARLIO: Invalid ring_index=" << ring_index << " (max=" << PARLIO_RING_BUFFER_COUNT << ")");
        return false;
    }

    if (start_led + led_count > mState.mIsrContext->total_leds) {
        FL_WARN("PARLIO: Ring buffer LED range overflow: start=" << start_led
                << " count=" << led_count << " total=" << mState.mIsrContext->total_leds);
        return false;
    }

    // Get ring buffer pointer
    uint8_t* outputBuffer = mState.ring_buffers[ring_index].get();
    if (!outputBuffer) {
        FL_WARN("PARLIO: Ring buffer " << ring_index << " not allocated");
        return false;
    }

    // Zero output buffer to prevent garbage data
    fl::memset(outputBuffer, 0x00, mState.ring_buffer_capacity);

    // ITERATION 3: Remove padding logic - buffer alignment is the solution
    // Previous iterations tried to fix corruption with padding, but this was wrong.
    // Padding adds LOW pulses that become part of the waveform, confusing the decoder.
    // The real solution is to ensure buffer boundaries align with LED frame boundaries.
    // With correct buffer sizing (156 LEDs × 24 bytes = 3744 bytes), the DMA gap
    // happens naturally BETWEEN LED frames, which WS2812B tolerates.
    size_t outputIdx = 0;

    // Get waveform generation resources
    uint8_t* laneWaveforms = mState.waveform_expansion_buffer.get();
    const uint8_t* bit0_wave = mState.bit0_waveform.data();
    const uint8_t* bit1_wave = mState.bit1_waveform.data();
    size_t waveform_size = mState.pulses_per_bit;

    const size_t max_pulses_per_bit = 16;
    const size_t bytes_per_lane = 8 * max_pulses_per_bit;

    // ITERATION 2: outputIdx now initialized above (with front padding logic)
    // Removed duplicate: size_t outputIdx = 0;

    // Process each LED in this buffer
    for (size_t ledIdx = 0; ledIdx < led_count; ledIdx++) {
        size_t absoluteLedIdx = start_led + ledIdx;

        // Process each color channel (R, G, B)
        for (size_t colorIdx = 0; colorIdx < 3; colorIdx++) {
            size_t byteOffset = absoluteLedIdx * 3 + colorIdx;

            // Step 1: Expand each lane's byte to waveform
            for (size_t lane = 0; lane < mState.data_width; lane++) {
                uint8_t* laneWaveform = laneWaveforms + (lane * bytes_per_lane);

                if (lane < mState.actual_channels) {
                    // Real channel - expand waveform from scratch buffer
                    const uint8_t* laneData = mState.scratch_padded_buffer.data() + (lane * mState.lane_stride);
                    uint8_t byte = laneData[byteOffset];

                    size_t expanded = fl::expandByteToWaveforms(
                        byte, bit0_wave, waveform_size, bit1_wave, waveform_size,
                        laneWaveform, bytes_per_lane);

                    if (expanded == 0) {
                        FL_WARN("PARLIO: Waveform expansion failed at LED " << absoluteLedIdx);
                        return false;
                    }
                } else {
                    // Dummy lane - zero waveform (keeps GPIO LOW)
                    fl::memset(laneWaveform, 0x00, bytes_per_lane);
                }
            }

            // Step 2: Bit-pack transpose waveform bytes to PARLIO format
            const size_t pulsesPerByte = 8 * mState.pulses_per_bit;

            if (mState.data_width <= 8) {
                // Pack into single bytes
                size_t ticksPerByte = 8 / mState.data_width;
                size_t numOutputBytes = (pulsesPerByte + ticksPerByte - 1) / ticksPerByte;

                for (size_t byteIdx = 0; byteIdx < numOutputBytes; byteIdx++) {
                    uint8_t outputByte = 0;

                    for (size_t t = 0; t < ticksPerByte; t++) {
                        size_t tick = byteIdx * ticksPerByte + t;
                        if (tick >= pulsesPerByte) break;

                        for (size_t lane = 0; lane < mState.data_width; lane++) {
                            uint8_t* laneWaveform = laneWaveforms + (lane * bytes_per_lane);
                            uint8_t pulse = laneWaveform[tick];
                            uint8_t bit = (pulse != 0) ? 1 : 0;

                            size_t bitPos = t * mState.data_width + lane;
                            outputByte |= (bit << bitPos);
                        }
                    }

                    // Check for buffer overflow
                    if (outputIdx + 1 > mState.ring_buffer_capacity) {
                        FL_WARN("PARLIO: Ring buffer overflow at outputIdx=" << outputIdx);
                        return false;
                    }

                    outputBuffer[outputIdx++] = outputByte;
                }
            } else if (mState.data_width == 16) {
                // Pack into 16-bit words
                for (size_t tick = 0; tick < pulsesPerByte; tick++) {
                    uint16_t outputWord = 0;

                    for (size_t lane = 0; lane < 16; lane++) {
                        uint8_t* laneWaveform = laneWaveforms + (lane * bytes_per_lane);
                        uint8_t pulse = laneWaveform[tick];
                        uint8_t bit = (pulse != 0) ? 1 : 0;
                        outputWord |= (bit << lane);
                    }

                    // Check for buffer overflow
                    if (outputIdx + 2 > mState.ring_buffer_capacity) {
                        FL_WARN("PARLIO: Ring buffer overflow at outputIdx=" << outputIdx);
                        return false;
                    }

                    outputBuffer[outputIdx++] = outputWord & 0xFF;
                    outputBuffer[outputIdx++] = (outputWord >> 8) & 0xFF;
                }
            }
        }
    }

    // ITERATION 3: Remove back padding - not needed with correct buffer alignment
    // Previous iterations added padding to "absorb" the DMA gap, but this was misguided.
    // The correct solution is to ensure buffers end exactly on LED frame boundaries.
    // With 156 LEDs × 24 bytes/LED = 3744 bytes, buffer ends after LED 155 completes.
    // The DMA gap then occurs BETWEEN LED 155 and LED 156 (between frames), which
    // WS2812B protocol tolerates naturally (per LOOP.md line 16).

    // Store actual size of this buffer
    mState.ring_buffer_sizes[ring_index] = outputIdx;

    // DEBUG: Copy DMA buffer data to debug deque for validation
    if (mState.mIsrContext) {
        // Append this buffer's data to the debug deque (push_back each byte)
        for (size_t i = 0; i < outputIdx; i++) {
            mState.mIsrContext->mDebugDmaOutput.push_back(outputBuffer[i]);
        }
    }

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
    if (!mInitialized || mState.tx_unit == nullptr || !mState.mIsrContext) {
        return EngineState::READY;
    }

    // Check for ISR-reported errors
    if (mState.error_occurred) {
        FL_WARN("PARLIO: Error occurred during streaming transmission");
        mState.mIsrContext->transmitting = false;
        mState.error_occurred = false;
        return EngineState::ERROR;
    }

    // Phase 4: Check if streaming is complete (volatile read, no barrier yet)
    // Read stream_complete first (volatile field ensures fresh read from memory)
    if (mState.mIsrContext->stream_complete) {
        // Execute memory barrier to synchronize all ISR writes
        // This ensures all non-volatile ISR data (isr_count, bytes_transmitted, etc.) is visible
        PARLIO_MEMORY_BARRIER();

        // Clear completion flags BEFORE cleanup operations (volatile assignment)
        // This prevents race condition where new transmission could start during cleanup
        // Previous code cleared flags AFTER cleanup, allowing state corruption
        mState.mIsrContext->transmitting = false;
        mState.mIsrContext->stream_complete = false;

        // Wait for final chunk to complete
        esp_err_t err = parlio_tx_unit_wait_all_done(mState.tx_unit, 0);

        if (err == ESP_OK) {
            // FIX C5-1 PARTIAL REVERT: GPIO reset loop IS needed (Iteration 21 fix was correct)
            // Analysis showed Patterns B-D fail without GPIO stabilization between transmissions
            // PARLIO idle_value alone is insufficient to prevent GPIO glitches on subsequent starts
            // OPTIMIZATION: Reduced delay from 1ms → 100μs (10x faster, still prevents glitches)

            // Disable PARLIO to reset peripheral state
            err = parlio_tx_unit_disable(mState.tx_unit);
            if (err != ESP_OK) {
                FL_WARN("PARLIO: Failed to disable TX unit after transmission: " << err);
            }

            // Force all GPIO pins to LOW state
            for (size_t i = 0; i < mState.actual_channels; i++) {
                int pin = mState.pins[i];
                if (pin >= 0) {
                    gpio_set_level(static_cast<gpio_num_t>(pin), 0);
                }
            }

            // Reduced delay: 100μs instead of 1ms (10x faster, 2% overhead instead of 20%)
            fl::delayMicros(100);

            // Re-enable PARLIO for next transmission
            err = parlio_tx_unit_enable(mState.tx_unit);
            if (err != ESP_OK) {
                FL_WARN("PARLIO: Failed to re-enable TX unit: " << err);
                // Flags already cleared atomically at line 670-671
                return EngineState::ERROR;
            }

            // Clear transmitting channels on completion
            mTransmittingChannels.clear();
            return EngineState::READY;
        } else if (err == ESP_ERR_TIMEOUT) {
            // Final chunk still transmitting
            return EngineState::BUSY;
        } else {
            FL_WARN("PARLIO: Error waiting for final chunk: " << err);
            // Flags already cleared atomically at line 670-671
            return EngineState::ERROR;
        }
    }

    // If we're not transmitting, we're ready
    if (!mState.mIsrContext->transmitting) {
        return EngineState::READY;
    }

    // Phase 3 - Iteration 18: No ring refill needed (single buffer transmission)
    // Streaming in progress - wait for ISR to mark completion
    return EngineState::BUSY;
}

void ChannelEnginePARLIOImpl::beginTransmission(
    fl::span<const ChannelDataPtr> channelData) {
    // Validate channel data first (before initialization)
    if (channelData.size() == 0) {
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
        }

    // Fill remaining pins with -1 for dummy lanes (unused)
    for (size_t i = channel_count; i < mState.data_width; i++) {
        mState.pins.push_back(-1);
    }

    // FIX C1-3: Extract timing configuration BEFORE initializeIfNeeded()
    // This ensures waveform generation has correct timing parameters from the start
    // Previously, timing was set AFTER init, causing first frame to use uninitialized values (0,0,0)
    const ChipsetTimingConfig& timing = channelData[0]->getTiming();
    mState.timing_t1_ns = timing.t1_ns;
    mState.timing_t2_ns = timing.t2_ns;
    mState.timing_t3_ns = timing.t3_ns;

    // Ensure PARLIO peripheral is initialized
    // Note: initializeIfNeeded() uses mState.timing_* to configure waveforms
    initializeIfNeeded();

    // If initialization failed, abort
    if (!mInitialized || mState.tx_unit == nullptr || !mState.mIsrContext) {
        FL_WARN("PARLIO: Cannot transmit - initialization failed");
        return;
    }

    // Check if we're already transmitting
    if (mState.mIsrContext->transmitting) {
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
        return;
    }

    size_t numLeds = maxChannelSize / 3;
    // Prepare single segmented scratch buffer for all N lanes
    // Layout: [lane0_data][lane1_data]...[laneN_data]
    // Each lane is maxChannelSize bytes (padded to same size)
    // This is regular heap (non-DMA) - only output waveform buffers need DMA
    // capability

    size_t totalBufferSize = channelData.size() * maxChannelSize;
    mState.scratch_padded_buffer.resize(totalBufferSize);

    // FIX H3-4: Removed redundant scratch buffer zero-fill (1-2ms savings per frame)
    // Subsequent loop (lines 862-882) overwrites all bytes via memcpy + right-padding memset
    // Pattern D corruption (Iteration 22) was fixed by right-padding logic, not this memset

    mState.lane_stride = maxChannelSize;
    mState.num_lanes = channelData.size();
    // Phase 5: Also set num_lanes in ISR context (ISR needs this for bytes_transmitted calculation)
    mState.mIsrContext->num_lanes = channelData.size();


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
            fl::memcpy(laneDst, srcData.data(), dataSize);  // Copy actual data first
            fl::memset(laneDst + dataSize, 0, maxChannelSize - dataSize);  // Zero-fill remainder
        } else {
            // No padding needed - direct copy
            fl::memcpy(laneDst, srcData.data(), maxChannelSize);
        }
    }

    // Phase 0: Ring buffer streaming architecture with CPU-based waveform generation
    // Initialize IsrContext state for ring buffer streaming
    mState.mIsrContext->total_leds = numLeds;
    mState.mIsrContext->current_led = 0;
    mState.mIsrContext->stream_complete = false;
    mState.error_occurred = false;
    mState.mIsrContext->transmitting = false; // Will be set to true after first buffer submitted

    // DEBUG: Clear debug DMA output buffer for this transmission
    mState.mIsrContext->mDebugDmaOutput.clear();

    // Initialize ring buffer state
    mState.mIsrContext->ring_read_ptr = 0;
    mState.mIsrContext->ring_write_ptr = 0;
    mState.mIsrContext->ring_size = PARLIO_RING_BUFFER_COUNT;
    mState.mIsrContext->ring_error = false;

    // Initialize counters
    mState.mIsrContext->isr_count = 0;
    mState.mIsrContext->bytes_transmitted = 0;
    mState.mIsrContext->chunks_completed = 0;
    mState.mIsrContext->transmission_active = true;
    mState.mIsrContext->end_time_us = 0;

    // ITERATION 1: OLD buffer allocation (BROKEN - splits small strips into multiple buffers)
    // size_t leds_per_buffer = (numLeds + PARLIO_RING_BUFFER_COUNT - 1) / PARLIO_RING_BUFFER_COUNT;
    // size_t total_buffers = (numLeds + leds_per_buffer - 1) / leds_per_buffer;

    // ITERATION 2: NEW buffer allocation - single-buffer mode for Phase 0
    // Phase 0 requirement: ALL LEDs must fit in ONE buffer to avoid DMA gaps
    // For small strips (≤156 LEDs), use single buffer mode
    // For large strips (>156 LEDs), use multi-buffer ring mode
    size_t single_buffer_max_leds = mState.ring_buffer_capacity / 24; // 3744 bytes / 24 bytes per LED = 156 LEDs
    size_t leds_per_buffer;
    size_t total_buffers;

    if (numLeds <= single_buffer_max_leds) {
        // ITERATION 2: Phase 0 path - all LEDs fit in one buffer (NO DMA gaps)
        leds_per_buffer = numLeds;
        total_buffers = 1;
        FL_WARN("PARLIO: Single-buffer mode activated (" << numLeds << " LEDs fit in one buffer)");
    } else {
        // ITERATION 2: Phase 1+ path - multi-buffer streaming with DMA gaps
        leds_per_buffer = (numLeds + PARLIO_RING_BUFFER_COUNT - 1) / PARLIO_RING_BUFFER_COUNT;
        total_buffers = (numLeds + leds_per_buffer - 1) / leds_per_buffer;
        FL_WARN("PARLIO: Multi-buffer mode activated (" << numLeds << " LEDs across "
                << total_buffers << " buffers, " << leds_per_buffer << " LEDs per buffer)");
    }

    mState.mIsrContext->buffers_submitted = 0;
    mState.mIsrContext->buffers_completed = 0;
    mState.mIsrContext->buffers_total = total_buffers;

    // Phase 0: Pre-generate ALL buffers first, then submit ONLY first buffer
    // Per LOOP.md: "CPU will spin and yield until it detects that the next DMA buffer is ready to pre-compute"
    // Strategy: Pre-fill the ring with all buffers upfront, then let ISR drain them
    //
    // Why pre-fill? Ensures ISR always has buffers ready (no underflow risk)
    // ISR will submit buffers from ring as previous ones complete

    size_t current_led = 0;
    size_t buffers_generated = 0;

    FL_WARN("PARLIO: Pre-generating all " << total_buffers << " buffers into ring");

    // Pre-generate ALL buffers into ring (CPU work done upfront)
    while (current_led < numLeds && buffers_generated < total_buffers) {
        size_t remaining = numLeds - current_led;
        size_t this_led_count = (remaining < leds_per_buffer) ? remaining : leds_per_buffer;
        size_t buffer_idx = buffers_generated % PARLIO_RING_BUFFER_COUNT;

        // Generate buffer into ring
        if (!generateRingBuffer(buffer_idx, current_led, this_led_count)) {
            FL_WARN("PARLIO: Failed to generate ring buffer " << buffer_idx);
            mState.error_occurred = true;
            return;
        }

        FL_WARN("PARLIO: Generated buffer " << buffer_idx << " (LEDs " << current_led
                << "-" << (current_led + this_led_count - 1) << ", "
                << mState.ring_buffer_sizes[buffer_idx] << " bytes)");

        current_led += this_led_count;
        buffers_generated++;

        // Update write pointer (signals buffer is ready for ISR consumption)
        mState.mIsrContext->ring_write_ptr++;
    }

    FL_WARN("PARLIO: All " << buffers_generated << " buffers pre-generated");

    // Verify all buffers were generated
    if (buffers_generated != total_buffers) {
        FL_WARN("PARLIO: Buffer generation mismatch - generated " << buffers_generated
                << " but expected " << total_buffers);
        mState.error_occurred = true;
        return;
    }

    // Phase 0: Submit ONLY first buffer from CPU thread
    // ISR will submit all subsequent buffers as they complete
    size_t first_buffer_idx = 0;
    size_t first_buffer_size = mState.ring_buffer_sizes[first_buffer_idx];

    // ITERATION 5 - Experiment 1-2: idle_value = HIGH FAILED (100% error rate)
    // ITERATION 5 - Experiment 3: Revert to idle_value = LOW, test with sample_edge = NEG
    parlio_transmit_config_t tx_config = {};
    tx_config.idle_value = 0x0000; // Start with LOW (reverted)
    // tx_config.idle_value = 0xFFFF; // ITERATION 5: HIGH failed (disabled)

    esp_err_t err = parlio_tx_unit_transmit(mState.tx_unit,
                                             mState.ring_buffers[first_buffer_idx].get(),
                                             first_buffer_size * 8, &tx_config);

    if (err != ESP_OK) {
        FL_WARN("PARLIO: Failed to submit first buffer: " << err);
        mState.error_occurred = true;
        return;
    }

    FL_WARN("PARLIO: Submitted first buffer (" << first_buffer_size << " bytes)");

    // Update read pointer (first buffer submitted, ISR will submit rest)
    mState.mIsrContext->ring_read_ptr = 1; // ISR starts from buffer 1

    // Mark transmission started and buffer accounting
    mState.mIsrContext->transmitting = true;
    mState.mIsrContext->buffers_submitted = 1; // Only first buffer submitted by CPU

    FL_WARN("PARLIO: Transmission started - ISR will handle remaining " << (total_buffers - 1) << " buffers");
}

//=============================================================================
// Private Methods - Initialization
//=============================================================================

void ChannelEnginePARLIOImpl::initializeIfNeeded() {
    if (mInitialized) {
        return;
    }

    // Phase 4: Allocate ParlioIsrContext (cache-aligned, 64 bytes)
    // Must use new with alignas(64) to ensure proper alignment
    if (!mState.mIsrContext) {
        mState.mIsrContext = new ParlioIsrContext();
        ParlioIsrContext::setInstance(mState.mIsrContext);  // Set singleton reference
    }

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
    // Step 1: Generate precomputed waveforms using timing from chipset configuration
    // Timing parameters are extracted from the first channel in beginTransmission()
    // and stored in mState.timing_t1_ns, mState.timing_t2_ns, mState.timing_t3_ns
    //
    // For WS2812B-V5 (typical values):
    // - T1: 300ns (T0H - initial HIGH time)
    // - T2: 200ns (T1H-T0H - additional HIGH time for bit 1)
    // - T3: 500ns (T0L - LOW tail duration)
    //
    // Resulting waveforms (with 10.0 MHz = 100ns per pulse):
    // - Bit 0: T1 HIGH + T3 LOW = 300ns H + 500ns L = 3 + 5 pulses
    // - Bit 1: (T1+T2) HIGH + T3 LOW = 500ns H + 300ns L = 5 + 3 pulses
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
    // PHASE 9: Generate nibble lookup table for optimized waveform expansion
    bool lut_success = fl::generateNibbleLookupTable(
        mState.bit0_waveform.data(),
        mState.bit1_waveform.data(),
        mState.pulses_per_bit,
        mState.nibble_lut
    );
    if (!lut_success) {
        FL_WARN("PARLIO: Failed to generate nibble lookup table");
        mInitialized = false;
        return;
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
    // PHASE 1 OPTION E: Increase buffer size to reduce chunk count below ISR callback limit
    // Root cause: ESP-IDF PARLIO driver stops invoking ISR callbacks after 4 transmissions
    // Solution: Use larger buffers (18KB) to fit 768 LEDs/chunk → 1000 LEDs = 2 chunks (within limit)
    // Previous: 2KB buffer → 85 LEDs/chunk → 12 chunks (exceeds 4-callback limit)
    // New: 18KB buffer → 768 LEDs/chunk → 2 chunks (works within ISR callback limit)
    constexpr size_t target_buffer_size = 18432; // 18KB per buffer (768 LEDs @ 24 bytes/LED for 1-lane)
    mState.leds_per_chunk = target_buffer_size / bytes_per_led_calc;

    // Clamp to reasonable range
    if (mState.leds_per_chunk < 10)
        mState.leds_per_chunk = 10;
    if (mState.leds_per_chunk > 500)
        mState.leds_per_chunk = 500;

    // Step 3: Validate GPIO pins were provided by beginTransmission()
    // Pins are extracted from ChannelData and validated before initialization
    if (mState.pins.size() != mState.data_width) {
        FL_WARN("PARLIO: Pin configuration error - expected "
                << mState.data_width << " pins, got " << mState.pins.size());
        FL_WARN("  Pins must be provided via FastLED.addLeds<WS2812, PIN>() API");
        mInitialized = false;
        return;
    }

    // Step 4: Configure PARLIO TX unit
    parlio_tx_unit_config_t config = {};
    config.clk_src = PARLIO_CLK_SRC_DEFAULT;
    config.clk_in_gpio_num =
        static_cast<gpio_num_t>(-1); // Use internal clock, not external
    config.output_clk_freq_hz = PARLIO_CLOCK_FREQ_HZ;
    config.data_width = mState.data_width; // Runtime parameter
    // Phase 1 - Iteration 2: Set trans_queue_depth to 2 (minimum for ISR streaming)
    // PARLIO has 3 internal queues (READY, PROGRESS, COMPLETE) per README.md line 101
    // With depth=1, ISR callback can't queue next buffer (completed transaction still in COMPLETE queue)
    // With depth=2, allows: 1 in PROGRESS + 1 buffer for ISR to queue → prevents watchdog timeout
    // Testing showed depth=1 causes watchdog timeout; depth=2 resolves it
    config.trans_queue_depth = 2;
    // Phase 3 - Iteration 19: ESP-IDF PARLIO hardware limit is 65535 BITS per transmission
    // This is a hard hardware limit that cannot be exceeded
    // For transmissions larger than this, we must chunk into multiple calls
    // Set max_transfer_size to hardware maximum (must be < 65535, so use 65534)
    config.max_transfer_size = 65534; // Hardware maximum (65535 bits - 1 for safety)
    config.bit_pack_order =
        PARLIO_BIT_PACK_ORDER_LSB; // Match bit-packing logic (bit 0 = tick 0)
    // ITERATION 5 - Experiment 2-3: sample_edge = NEG made things WORSE (39 corrupt LEDs vs 24)
    // ITERATION 5 - Reverting to POS (best configuration found in Iteration 1)
    config.sample_edge =
        PARLIO_SAMPLE_EDGE_POS; // ITERATION 1: Match ESP-IDF example (was NEG)
    // config.sample_edge =
    //     PARLIO_SAMPLE_EDGE_NEG; // ITERATION 5: Experiment 2-3 - NEG failed (disabled)

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

    // Step 6: Register ISR callback for streaming (MUST be done BEFORE enabling)
    // CRITICAL: ESP-IDF requires callbacks to be registered before parlio_tx_unit_enable()
    // Registering after enable causes callbacks to never fire
    parlio_tx_event_callbacks_t callbacks = {};
    callbacks.on_trans_done =
        reinterpret_cast<parlio_tx_done_callback_t>(txDoneCallback);

    err = parlio_tx_unit_register_event_callbacks(mState.tx_unit, &callbacks,
                                                  this);
    if (err != ESP_OK) {
        FL_WARN("PARLIO: Failed to register callbacks: " << err);
        parlio_del_tx_unit(mState.tx_unit);
        mState.tx_unit = nullptr;
        mInitialized = false;
        return;
    }

    // Step 7: Enable TX unit (AFTER callback registration)
    err = parlio_tx_unit_enable(mState.tx_unit);
    if (err != ESP_OK) {
        FL_WARN("PARLIO: Failed to enable TX unit: " << err);
        parlio_del_tx_unit(mState.tx_unit);
        mState.tx_unit = nullptr;
        mInitialized = false;
        return;
    }

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

    // ITERATION 3: FIX - The above calculation is WRONG for ring buffer sizing!
    // It calculates pulse count, not actual output bytes after bit-packing.
    // The transpose logic (lines 710-739) packs pulses into bytes:
    // - ticksPerByte = 8 / data_width (for data_width=1: 8 ticks per byte)
    // - numOutputBytes = pulsesPerByte / ticksPerByte
    // - For 1-lane: (8 bits × 8 pulses) / 8 = 8 bytes per color × 3 = 24 bytes/LED
    //
    // This bug caused ring buffers to be allocated at 192 bytes/LED (8x too large!)
    // when they should be 24 bytes/LED, causing buffer misalignment with LED frames.

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

    // Phase 0: Allocate ring buffers for streaming multi-buffer DMA
    // Calculate LEDs per ring buffer: distribute 1000 LEDs across 8 buffers
    // For 1000 LEDs / 8 buffers = 125 LEDs per buffer (last buffer may be smaller)
    size_t ring_buffer_led_count = (1000 + PARLIO_RING_BUFFER_COUNT - 1) / PARLIO_RING_BUFFER_COUNT; // Round up

    // ITERATION 3: Calculate correct bytes_per_led accounting for bit-packing
    // The transpose logic packs pulses into bytes based on data_width
    // Formula matches generateRingBuffer() transpose logic (lines 710-739):
    // - pulsesPerByte = 8 bits × pulses_per_bit
    // - ticksPerByte = 8 / data_width
    // - numOutputBytes = (pulsesPerByte + ticksPerByte - 1) / ticksPerByte
    // - bytes_per_led = numOutputBytes × 3 colors
    size_t bytes_per_led_ring;
    if (mState.data_width <= 8) {
        size_t ticksPerByte = 8 / mState.data_width;
        size_t pulsesPerByte = 8 * mState.pulses_per_bit;  // 8 bits per color byte
        size_t numOutputBytes = (pulsesPerByte + ticksPerByte - 1) / ticksPerByte;
        bytes_per_led_ring = numOutputBytes * 3;  // 3 colors (RGB)
    } else if (mState.data_width == 16) {
        // For 16-bit width: 2 bytes per pulse tick
        size_t pulsesPerByte = 8 * mState.pulses_per_bit;
        bytes_per_led_ring = pulsesPerByte * 2 * 3;  // 2 bytes per pulse × 3 colors
    } else {
        bytes_per_led_ring = 30;  // Fallback (should never happen)
    }

    size_t ring_buffer_capacity = ring_buffer_led_count * bytes_per_led_ring;

    mState.ring_buffer_capacity = ring_buffer_capacity;
    mState.ring_buffers.clear();
    mState.ring_buffer_sizes.clear();

    // Allocate all ring buffers
    for (size_t i = 0; i < PARLIO_RING_BUFFER_COUNT; i++) {
        fl::unique_ptr<uint8_t[], HeapCapsDeleter> buffer(
            static_cast<uint8_t*>(heap_caps_malloc(ring_buffer_capacity, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL))
        );

        if (!buffer) {
            FL_WARN("PARLIO: Failed to allocate ring buffer " << i << "/" << PARLIO_RING_BUFFER_COUNT
                    << " (requested " << ring_buffer_capacity << " bytes)");
            // Clean up already allocated ring buffers (automatic via unique_ptr destructors)
            mState.ring_buffers.clear();
            mState.ring_buffer_sizes.clear();

            // Clean up old buffers
            mState.buffer_a.reset();
            mState.buffer_b.reset();

            // Clean up TX unit
            esp_err_t disable_err = parlio_tx_unit_disable(mState.tx_unit);
            if (disable_err != ESP_OK) {
                FL_WARN("PARLIO: Failed to disable TX unit during ring buffer cleanup: " << disable_err);
            }
            esp_err_t del_err = parlio_del_tx_unit(mState.tx_unit);
            if (del_err != ESP_OK) {
                FL_WARN("PARLIO: Failed to delete TX unit during ring buffer cleanup: " << del_err);
            }
            mState.tx_unit = nullptr;
            mInitialized = false;
            return;
        }

        mState.ring_buffers.push_back(fl::move(buffer));
        mState.ring_buffer_sizes.push_back(0); // Will be set during generation
    }

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
    // Phase 4: Initialize IsrContext state (if allocated)
    if (mState.mIsrContext) {
        mState.mIsrContext->transmitting = false;
        mState.mIsrContext->stream_complete = false;
        mState.mIsrContext->current_led = 0;
        mState.mIsrContext->total_leds = 0;
    }
    mState.error_occurred = false;

    mInitialized = true;
    }

//=============================================================================
// Polymorphic Wrapper Class Implementation
//=============================================================================

ChannelEnginePARLIO::ChannelEnginePARLIO()
    : mEngine(), mCurrentDataWidth(0) {
    }

ChannelEnginePARLIO::~ChannelEnginePARLIO() {
    // fl::unique_ptr automatically deletes mEngine when it goes out of scope (RAII)
    mCurrentDataWidth = 0;
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
        mEngine = fl::make_unique<ChannelEnginePARLIOImpl>(required_width);
        mCurrentDataWidth = required_width;
    } else if (mCurrentDataWidth != required_width) {
        // Width changed - need to reconfigure
        // Reset unique_ptr - automatically deletes old engine, creates new one
        mEngine.reset(new ChannelEnginePARLIOImpl(required_width));
        mCurrentDataWidth = required_width;
        need_reconfigure = true;
    }

    // Log configuration info
    if (!need_reconfigure && mEngine) {
        }

    // Delegate transmission to the single engine
    for (const auto &channel : channelData) {
        mEngine->enqueue(channel);
    }

    mEngine->show();
}

//=============================================================================
// Debug Instrumentation Implementation (Phase 0)
//=============================================================================

ParlioDebugMetrics getParlioDebugMetrics() {
    // Use singleton accessor to get ISR context
    ParlioDebugMetrics metrics = {};

    // Get ISR context from singleton
    auto* ctx = ParlioIsrContext::getInstance();
    if (!ctx) {
        return metrics; // Return zeros if not initialized
    }

    // Read buffer accounting from ISR context
    metrics.isr_count = ctx->buffers_completed;
    metrics.chunks_queued = ctx->buffers_submitted;
    metrics.chunks_completed = ctx->buffers_completed;

    // Calculate bytes from buffer counts
    // Note: This is approximate since buffers may have different sizes
    // For accurate tracking, we'd need to sum ring_buffer_sizes
    size_t bytes_per_led = 24; // RGB: 3 colors × 8 bits × 8 pulses / 8 bits/byte = 24 bytes
    size_t leds_per_buffer = (ctx->buffers_total > 0) ? (ctx->total_leds / ctx->buffers_total) : 0;
    if (ctx->buffers_total > 0) {
        metrics.bytes_total = ctx->total_leds * bytes_per_led;
        metrics.bytes_transmitted = ctx->buffers_completed * leds_per_buffer * bytes_per_led;
    }

    metrics.transmission_active = ctx->transmitting;
    metrics.start_time_us = 0; // Not currently tracked
    metrics.end_time_us = ctx->end_time_us;
    metrics.error_code = 0; // ESP_OK

    return metrics;
}

//=============================================================================
// Factory Function Implementation
//=============================================================================

fl::shared_ptr<IChannelEngine> createParlioEngine() {
    return fl::make_shared<ChannelEnginePARLIO>();
}

} // namespace fl

#endif // FASTLED_ESP32_HAS_PARLIO
#endif // ESP32
