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
// Phase 4: ISR Context - Cache-Aligned Structure for Optimal Performance
//=============================================================================
// All ISR-related state consolidated into single 64-byte aligned struct for:
// - Improved cache line performance (single cache line access)
// - Prevention of false sharing between ISR and main thread
// - Clear separation between volatile (ISR-shared) and non-volatile fields
//
// Memory Synchronization Model:
// - ISR writes to volatile fields (stream_complete, transmitting, current_led)
// - Main thread reads volatile fields directly (compiler ensures fresh read)
// - After detecting stream_complete==true, main thread executes memory barrier
// - Memory barrier ensures all ISR writes visible before reading other fields
// - Non-volatile fields (isr_count, etc.) read after barrier are guaranteed consistent
//
// Cross-Platform Memory Barriers:
// - Xtensa (ESP32, ESP32-S3): asm volatile("memw" ::: "memory")
// - RISC-V (ESP32-C6, ESP32-C3): asm volatile("fence rw, rw" ::: "memory")
//
alignas(64) struct ParlioIsrContext {
    // === Volatile Fields (shared between ISR and main thread) ===
    // These fields are written by ISR, read by main thread
    // Marked volatile to prevent compiler optimizations that would cache values

    volatile bool stream_complete;  ///< ISR sets true when transmission complete (ISR writes, main reads)
    volatile bool transmitting;     ///< ISR updates during transmission lifecycle (ISR writes, main reads)
    volatile size_t current_led;    ///< ISR updates LED position marker (ISR writes, main reads)

    // === Non-Volatile Fields (main thread synchronization required) ===
    // These fields are written by ISR but read by main thread ONLY after memory barrier
    // NOT marked volatile - main thread uses explicit barrier after reading stream_complete

    size_t total_leds;              ///< Total LEDs to transmit (main sets, ISR reads - no race)
    size_t num_lanes;               ///< Number of parallel lanes (main sets, ISR reads - Phase 5: moved from ParlioState)

    // Atomic counters (use __atomic_* intrinsics for thread-safe increment)
    // Note: Not using std::atomic to avoid function call overhead in ISR
    uint32_t isr_count;             ///< ISR callback invocation count
    uint32_t bytes_transmitted;     ///< Total bytes transmitted this frame
    uint32_t chunks_completed;      ///< Number of chunks completed this frame

    // Diagnostic fields (written by ISR, read by main thread after barrier)
    bool transmission_active;       ///< Debug: Transmission currently active
    uint64_t end_time_us;           ///< Debug: Transmission end timestamp (microseconds)

    // Constructor: Initialize all fields to safe defaults
    ParlioIsrContext()
        : stream_complete(false)
        , transmitting(false)
        , current_led(0)
        , total_leds(0)
        , num_lanes(0)
        , isr_count(0)
        , bytes_transmitted(0)
        , chunks_completed(0)
        , transmission_active(false)
        , end_time_us(0)
    {}
};

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

    }

ChannelEnginePARLIOImpl::~ChannelEnginePARLIOImpl() {
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

    // Phase 4: Clean up IsrContext
    if (mState.mIsrContext) {
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

// Phase 4: O3 optimization attribute for maximum ISR performance
// Applies aggressive inlining, loop unrolling, and instruction scheduling
__attribute__((optimize("O3")))
bool IRAM_ATTR ChannelEnginePARLIOImpl::txDoneCallback(
    parlio_tx_unit_handle_t tx_unit, const void *edata, void *user_ctx) {
    // Phase 3 - Iteration 18: Simplified ISR callback (single buffer transmission)
    // Phase 4 - Iteration 26: Migrated to IsrContext for cache-aligned performance
    // Entire waveform transmitted in one shot, ISR only marks completion
    (void)edata;
    (void)tx_unit;
    auto *self = static_cast<ChannelEnginePARLIOImpl *>(user_ctx);
    if (!self || !self->mState.mIsrContext) {
        return false;
    }

    // Phase 4: Access ISR state via cache-aligned ParlioIsrContext struct
    ParlioIsrContext* ctx = self->mState.mIsrContext;

    // Update ISR counter (atomic operation for thread safety)
    __atomic_add_fetch(&ctx->isr_count, 1, __ATOMIC_RELAXED);

    // FIX C2-1: Use atomic store for current_led (prevents torn reads on 64-bit platforms)
    // FIX C2-2: Use RELEASE ordering for all completion markers (ensures main thread sees all ISR writes)
    __atomic_store_n(&ctx->current_led, ctx->total_leds, __ATOMIC_RELEASE);
    __atomic_store_n(&ctx->stream_complete, true, __ATOMIC_RELEASE);
    __atomic_store_n(&ctx->transmitting, false, __ATOMIC_RELEASE);

    // Update diagnostics (atomic with RELEASE ordering for consistency)
    // Phase 5: Use ctx->num_lanes instead of self->mState.num_lanes (ISR context separation)
    size_t bytes_transmitted = ctx->total_leds * 3 * ctx->num_lanes;
    __atomic_add_fetch(&ctx->bytes_transmitted, bytes_transmitted, __ATOMIC_RELEASE);
    __atomic_add_fetch(&ctx->chunks_completed, 1, __ATOMIC_RELEASE);
    ctx->transmission_active = false;
    ctx->end_time_us = micros();

    // ISR complete - transmission finished
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
    parlio_transmit_config_t tx_config = {};
    tx_config.idle_value = 0x0000;

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

        // FIX C1-2: Clear completion flags atomically BEFORE cleanup operations
        // This prevents race condition where new transmission could start during cleanup
        // Previous code cleared flags AFTER cleanup, allowing state corruption
        __atomic_store_n(&mState.mIsrContext->transmitting, false, __ATOMIC_RELEASE);
        __atomic_store_n(&mState.mIsrContext->stream_complete, false, __ATOMIC_RELEASE);

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

    // Phase 3 - Iteration 18: Single large buffer architecture (no ring buffer)
    // Calculate required buffer size for entire transmission
    // Each LED: 3 bytes (RGB) × 8 bits × pulses_per_bit = 24 × pulses_per_bit pulses
    // For WS2812B at 8MHz: 10 pulses/bit → 240 pulses/LED
    // Packed format depends on data_width
    const size_t bytes_per_led = (3 * 8 * mState.pulses_per_bit * mState.data_width) / 8;
    const size_t led_data_bytes = numLeds * bytes_per_led;

    // ITERATION 23: Add minimal padding to ensure final pulse completes
    // Root cause: ESP-IDF PARLIO immediately returns GPIO to idle_value when transmission completes,
    // which can truncate the final HIGH pulse if it's still in progress
    // Solution: Add 1 byte (8 pulses = 1μs) of LOW padding to let final pulse complete
    // Note: Full WS2812B RESET (280μs) not needed - LEDs latch correctly with minimal padding
    // 8190 (LED data) + 1 (padding) = 8191 bytes (exactly at single-chunk limit, no chunking)
    const size_t padding_bytes = 1;  // 1 byte = 8 pulses = 1μs at 8MHz

    const size_t required_buffer_size = led_data_bytes + padding_bytes;

    // Reallocate buffer_a if current size is insufficient
    if (!mState.buffer_a || mState.buffer_size < required_buffer_size) {
        mState.buffer_a.reset(static_cast<uint8_t*>(
            heap_caps_malloc(required_buffer_size, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL)));

        if (!mState.buffer_a) {
            FL_WARN("PARLIO: Failed to allocate buffer_a (" << required_buffer_size << " bytes)");
            return;
        }

        mState.buffer_size = required_buffer_size;
        }

    // Zero output buffer to prevent garbage data
    fl::memset(mState.buffer_a.get(), 0x00, required_buffer_size);

    // Phase 4: Initialize IsrContext state
    mState.mIsrContext->total_leds = numLeds;
    mState.mIsrContext->current_led = 0;
    mState.mIsrContext->stream_complete = false;
    mState.error_occurred = false; // error_occurred is still in ParlioState (not ISR-critical)
    mState.mIsrContext->transmitting = true;

    // Phase 4: Initialize IsrContext counters (non-atomic initialization, safe before ISR starts)
    mState.mIsrContext->isr_count = 0;
    mState.mIsrContext->bytes_transmitted = 0;
    mState.mIsrContext->chunks_completed = 0;
    mState.mIsrContext->transmission_active = true;
    mState.mIsrContext->end_time_us = 0;

    // Initialize debug metrics (legacy global, for backward compatibility)

    // Generate complete waveform into buffer_a
    size_t outputIdx = 0;
    uint8_t *outputBuffer = mState.buffer_a.get();
    uint8_t *laneWaveforms = mState.waveform_expansion_buffer.get();

    const uint8_t *bit0_wave = mState.bit0_waveform.data();
    const uint8_t *bit1_wave = mState.bit1_waveform.data();
    size_t waveform_size = mState.pulses_per_bit;

    const size_t max_pulses_per_bit = 16;
    const size_t bytes_per_lane = 8 * max_pulses_per_bit;

    // Process each LED
    for (size_t ledIdx = 0; ledIdx < numLeds; ledIdx++) {
        // Process each color channel (R, G, B)
        for (size_t colorIdx = 0; colorIdx < 3; colorIdx++) {
            size_t byteOffset = ledIdx * 3 + colorIdx;

            // FIX H5-2: Removed redundant memset (15% buffer prep speedup)
            // expandByteToWaveforms() overwrites all bytes, no need to zero first
            // This was inside triple-nested loop (300× per frame), wasting ~600KB writes

            // Step 1: Expand each lane's byte to waveform
            for (size_t lane = 0; lane < mState.data_width; lane++) {
                uint8_t *laneWaveform = laneWaveforms + (lane * bytes_per_lane);

                if (lane < mState.actual_channels) {
                    // Real channel - expand waveform from scratch buffer
                    const uint8_t *laneData = mState.scratch_padded_buffer.data() + (lane * mState.lane_stride);
                    uint8_t byte = laneData[byteOffset];

                    size_t expanded = fl::expandByteToWaveforms(
                        byte, bit0_wave, waveform_size, bit1_wave, waveform_size,
                        laneWaveform, bytes_per_lane);

                    if (expanded == 0) {
                        FL_WARN("PARLIO: Waveform expansion failed at LED " << ledIdx);
                        mState.error_occurred = true;
                        mState.mIsrContext->transmitting = false;
                        return;
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
                            uint8_t *laneWaveform = laneWaveforms + (lane * bytes_per_lane);
                            uint8_t pulse = laneWaveform[tick];
                            uint8_t bit = (pulse != 0) ? 1 : 0;

                            size_t bitPos = t * mState.data_width + lane;
                            outputByte |= (bit << bitPos);
                        }
                    }

                    // FIX C3-2: Check for buffer overflow BEFORE writing (prevent corruption)
                    if (outputIdx + 1 > required_buffer_size) {
                        FL_WARN("PARLIO: Buffer overflow prevented at outputIdx=" << outputIdx);
                        mState.error_occurred = true;
                        mState.mIsrContext->transmitting = false;
                        return;
                    }

                    outputBuffer[outputIdx++] = outputByte;
                }
            } else if (mState.data_width == 16) {
                // Pack into 16-bit words
                for (size_t tick = 0; tick < pulsesPerByte; tick++) {
                    uint16_t outputWord = 0;

                    for (size_t lane = 0; lane < 16; lane++) {
                        uint8_t *laneWaveform = laneWaveforms + (lane * bytes_per_lane);
                        uint8_t pulse = laneWaveform[tick];
                        uint8_t bit = (pulse != 0) ? 1 : 0;
                        outputWord |= (bit << lane);
                    }

                    // FIX C3-2: Check for buffer overflow BEFORE writing (prevent corruption)
                    if (outputIdx + 2 > required_buffer_size) {
                        FL_WARN("PARLIO: Buffer overflow prevented at outputIdx=" << outputIdx);
                        mState.error_occurred = true;
                        mState.mIsrContext->transmitting = false;
                        return;
                    }

                    outputBuffer[outputIdx++] = outputWord & 0xFF;
                    outputBuffer[outputIdx++] = (outputWord >> 8) & 0xFF;
                }
            }
        }
    }

    // FIX C3-2: Removed redundant buffer overflow check (now checked BEFORE each write)
    // Previously checked AFTER all writes completed, which was too late to prevent corruption

    // ITERATION 23: Add minimal padding (1μs LOW) to ensure final pulse completes
    // This prevents end-of-transmission truncation that corrupts final LED
    // Padding already allocated in buffer (padding_bytes = 1 byte calculated earlier)
    fl::memset(outputBuffer + outputIdx, 0x00, padding_bytes);
    outputIdx += padding_bytes;
    // ESP-IDF PARLIO enforces a maximum transfer size of 65535 bits (hardware + software limit)
    // The max_transfer_size config parameter must be set to <= 65534 bits
    // For chunking, we calculate max bytes: 65534 bits / 8 = 8191.75 bytes → 8191 bytes max
    // 8191 bytes = 65528 bits (safely under 65534 bit limit)

    const size_t MAX_TRANSFER_BYTES = 8191;  // 65528 bits (< 65534 hardware limit)
    const size_t totalBytes = outputIdx;

    if (totalBytes <= MAX_TRANSFER_BYTES) {
        // Single transmission - no chunking needed
        parlio_transmit_config_t tx_config = {};
        tx_config.idle_value = 0x0000;

        size_t buffer_bits = totalBytes * 8;
        esp_err_t err = parlio_tx_unit_transmit(mState.tx_unit, outputBuffer, buffer_bits, &tx_config);

        if (err != ESP_OK) {
            FL_WARN("PARLIO: Failed to transmit buffer: " << err);
            mState.error_occurred = true;
            mState.mIsrContext->transmitting = false;
            return;
        }

        } else {
        // Multi-chunk transmission - queue all chunks upfront
        const size_t numChunks = (totalBytes + MAX_TRANSFER_BYTES - 1) / MAX_TRANSFER_BYTES;

        parlio_transmit_config_t tx_config = {};
        tx_config.idle_value = 0x0000;

        size_t bytesQueued = 0;

        for (size_t chunkIdx = 0; chunkIdx < numChunks; chunkIdx++) {
            size_t chunkOffset = chunkIdx * MAX_TRANSFER_BYTES;
            size_t remainingBytes = totalBytes - chunkOffset;
            size_t chunkBytes = (remainingBytes < MAX_TRANSFER_BYTES) ? remainingBytes : MAX_TRANSFER_BYTES;
            size_t chunkBits = chunkBytes * 8;

            uint8_t *chunkBuffer = outputBuffer + chunkOffset;

            esp_err_t err = parlio_tx_unit_transmit(mState.tx_unit, chunkBuffer, chunkBits, &tx_config);

            if (err != ESP_OK) {
                FL_WARN("PARLIO: Failed to transmit chunk " << chunkIdx << "/" << numChunks
                        << " (offset: " << chunkOffset << ", bytes: " << chunkBytes << "): " << err);
                mState.error_occurred = true;
                mState.mIsrContext->transmitting = false;
                return;
            }

            bytesQueued += chunkBytes;
            }

        }
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
    // - T1: 225ns (T0H - initial HIGH time)
    // - T2: 355ns (T1H-T0H - additional HIGH time for bit 1)
    // - T3: 645ns (T0L - LOW tail duration)
    //
    // Resulting waveforms (with 8.0 MHz = 125ns per pulse):
    // - Bit 0: T1 HIGH + T3 LOW = 225ns H + 645ns L ≈ 2 + 5 pulses
    // - Bit 1: (T1+T2) HIGH + T3 LOW = 580ns H + 645ns L ≈ 5 + 5 pulses
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
    // Solution: Use larger buffers (18KB) to fit 75 LEDs/chunk → 300 LEDs = 4 chunks (within limit)
    // Previous: 2KB buffer → 10 LEDs/chunk → 30 chunks (exceeds 4-callback limit)
    // New: 18KB buffer → 75 LEDs/chunk → 4 chunks (works within ISR callback limit)
    constexpr size_t target_buffer_size = 18432; // 18KB per buffer (75 LEDs @ 240 bytes/LED for 1-lane)
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
    config.trans_queue_depth =
        8; // PHASE 1: Increased from 4 to 8 for minimum ISR gap (per PARLIO_INFO.md)
    // Phase 3 - Iteration 19: ESP-IDF PARLIO hardware limit is 65535 BITS per transmission
    // This is a hard hardware limit that cannot be exceeded
    // For transmissions larger than this, we must chunk into multiple calls
    // Set max_transfer_size to hardware maximum (must be < 65535, so use 65534)
    config.max_transfer_size = 65534; // Hardware maximum (65535 bits - 1 for safety)
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

    // Step 6: Enable TX unit
    err = parlio_tx_unit_enable(mState.tx_unit);
    if (err != ESP_OK) {
        FL_WARN("PARLIO: Failed to enable TX unit: " << err);
        parlio_del_tx_unit(mState.tx_unit);
        mState.tx_unit = nullptr;
        mInitialized = false;
        return;
    }

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
    // Debug metrics removed in Phase 21 - return zeroed structure
    ParlioDebugMetrics metrics = {};
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
