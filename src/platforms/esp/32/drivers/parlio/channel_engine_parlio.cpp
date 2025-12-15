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
#include "fl/error.h"
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
#include "soc/soc_caps.h" // For SOC_PARLIO_TX_UNIT_MAX_DATA_WIDTH
FL_EXTERN_C_END

#ifndef CONFIG_PARLIO_TX_ISR_HANDLER_IN_IRAM
#warning                                                                       \
    "PARLIO: CONFIG_PARLIO_TX_ISR_HANDLER_IN_IRAM is not defined! Add 'build_flags = -DCONFIG_PARLIO_TX_ISR_HANDLER_IN_IRAM=1' to platformio.ini or your build system"
#endif

namespace fl {

//=============================================================================
// Constants
//=============================================================================

// WS2812 timing requirements with PARLIO
// Clock: 10.0 MHz (100ns per tick)
// Divides from PLL_F160M on ESP32-P4 (160/16) or PLL_F240M on ESP32-C6 (240/24)
// Each LED bit is encoded as 8 clock ticks (0.8μs total)
static constexpr uint32_t PARLIO_CLOCK_FREQ_HZ =
    10000000; // 10.0 MHz

//=============================================================================
// Phase 4: Cross-Platform Memory Barriers
//=============================================================================
// Memory barriers ensure correct synchronization between ISR and main thread
// - ISR writes to volatile fields (stream_complete, transmitting, current_led)
// - Main thread reads volatile fields, then executes barrier before reading
// non-volatile fields
// - Barrier ensures all ISR writes are visible to main thread
//
// Platform-Specific Barriers:
// - Xtensa (ESP32, ESP32-S3): memw instruction (memory write barrier)
// - RISC-V (ESP32-C6, ESP32-C3, ESP32-H2): fence rw,rw instruction (full memory
// fence)
//
#if defined(__XTENSA__)
              // Xtensa architecture (ESP32, ESP32-S3)
#define PARLIO_MEMORY_BARRIER() asm volatile("memw" ::: "memory")
#elif defined(__riscv)
              // RISC-V architecture (ESP32-C6, ESP32-C3, ESP32-H2, ESP32-P4)
#define PARLIO_MEMORY_BARRIER() asm volatile("fence rw, rw" ::: "memory")
#else
#error                                                                         \
    "Unsupported architecture for PARLIO memory barrier (expected __XTENSA__ or __riscv)"
#endif

//=============================================================================
// Singleton Instance Definition
//=============================================================================
// ParlioIsrContext singleton - defined in header, instance defined here
ParlioIsrContext *ParlioIsrContext::s_instance = nullptr;

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

/// @brief Calculate total DMA buffer size for byte stream
/// @param num_bytes Number of input bytes per lane
/// @param data_width Number of parallel lanes (1, 2, 4, 8, or 16)
/// @return Total buffer size in bytes (after wave8 expansion + transpose)
/// @note Formula: buffer_size = (num_bytes × 64 bits × data_width + 7) / 8
/// bytes
/// @note Each input byte → 64 bits output (8 bits × 8 pulses per bit)
static inline size_t calculateTotalBufferSize(size_t num_bytes,
                                              size_t data_width) {
    // Each input byte generates 64 bits of waveform (8 bits × 8 pulses/bit)
    // Result: 8 bytes per input byte per lane, scaled by data_width for
    // parallel transmission
    return (num_bytes * 64 * data_width + 7) / 8;
}

/// @brief Calculate DMA chunk buffer size for ring buffer streaming
/// @param total_bytes Total number of input bytes per lane to transmit
/// @param num_chunks Number of ring buffers to split transmission into
/// @param data_width Number of parallel lanes (1, 2, 4, 8, or 16)
/// @return Size of each chunk buffer in bytes (last chunk may be smaller)
/// @note VALIDATION REQUIRED: Verify chunk size fits within PARLIO hardware
/// limits (65535 bytes max)
/// @note Last chunk will be smaller if total_bytes not evenly divisible by
/// num_chunks
static inline size_t calculateChunkBufferSize(size_t total_bytes,
                                              size_t num_chunks,
                                              size_t data_width) {
    // Calculate bytes per chunk (round up for buffer allocation)
    size_t bytes_per_chunk = (total_bytes + num_chunks - 1) / num_chunks;

    // Use official formula for chunk size
    return calculateTotalBufferSize(bytes_per_chunk, data_width);
}

//-----------------------------------------------------------------------------
// ISR Transposition Algorithm Design
//-----------------------------------------------------------------------------
//
// PARLIO hardware transmits data in parallel across multiple GPIO pins.
// We use wave8 for byte-level waveform generation with custom bit-packing
// for PARLIO's parallel format.
//
// INPUT FORMAT (per-strip layout):
//   Strip 0: [byte0, byte1, byte2, byte3, byte4, byte5, ...]
//   Strip 1: [byte0, byte1, byte2, byte3, byte4, byte5, ...]
//   ...
//   Strip 7: [byte0, byte1, byte2, byte3, byte4, byte5, ...]
//   Note: Bytes could be RGB data, RGBW data, preambles, or any byte stream
//
// ISR PROCESSING (per byte):
//   For each byte position:
//     1. Read byte from all 8 lanes (per-strip layout with stride)
//     2. Expand each byte using wave8() with precomputed timing LUT
//        - Each byte → Wave8Byte (8 bytes, 64 pulses total)
//     3. Bit-pack transpose: convert Wave8Byte to PARLIO's bit-packed format
//        - Each pulse → 1 bit, packed across lanes
//
// OUTPUT FORMAT (bit-parallel, 8 lanes):
//   Byte 0:  [S7_p0, S6_p0, ..., S1_p0, S0_p0]  // Pulse 0 from all strips
//   Byte 1:  [S7_p1, S6_p1, ..., S1_p1, S0_p1]  // Pulse 1 from all strips
//   ...
//   Byte 63: [S7_p63, S6_p63, ..., S1_p63, S0_p63]  // Pulse 63 from all strips
//   (Pattern repeats for each byte in the stream)
//
// MEMORY LAYOUT:
//   For 8 lanes:
//     - Each input byte → 8 bytes output (after wave8 + transpose)
//
// BUFFER SIZE CALCULATION:
//   Scratch buffer: N bytes × 8 strips (per-strip layout)
//   DMA chunks: M bytes × 8 bytes/byte = M×8 bytes (per buffer)
//
// EXAMPLE (300 bytes, 8 strips):
//   Scratch buffer: 300 × 8 = 2,400 bytes (per-strip layout)
//   DMA buffers: 2 × (100 × 8) = 1,600 bytes (ping-pong expanded waveforms)
//   Total: ~4 KB (waveform expansion happens in ISR, not stored)
//
// CHUNKING STRATEGY:
//   PARLIO has 65535 byte max transfer size. For large byte streams:
//   - Process bytes generically (no assumptions about RGB structure)
//   - Chunk size: Variable based on buffer capacity
//   - Max ~8191 input bytes per chunk (65535 / 8 bytes per byte)
//   - Transmit chunks sequentially via ping-pong DMA buffering
//
// WAVEFORM GENERATION:
//   Uses wave8 with configurable timing parameters:
//   - T1: T0H (initial HIGH time)
//   - T2: T1H-T0H (additional HIGH time for bit 1)
//   - T3: T0L (LOW tail duration)
//   - Clock: 10.0 MHz (100ns per pulse)
//   - Result: 8 pulses per bit (wave8 fixed 8:1 expansion)
//
//-----------------------------------------------------------------------------

//=============================================================================
// Global Instance Tracker
//=============================================================================

// Phase 1: Global instance tracker for debug metrics access
// This is set by ChannelEnginePARLIOImpl constructor/destructor
static ChannelEnginePARLIOImpl *g_parlio_instance = nullptr;

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
        ParlioIsrContext::setInstance(nullptr); // Clear singleton reference
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
// Per LOOP.md line 36: "parlio_tx_unit_transmit() must be called from the main
// thread (CPU) only once, after this it must be called from the on done
// callback ISR until the last buffer"
//
// ISR responsibilities:
//   1. Increment buffers_completed counter (signals CPU one buffer finished)
//   2. Check if more buffers need to be submitted
//   3. If yes: Submit next buffer from ring to hardware via
//   parlio_tx_unit_transmit()
//   4. Update ring_read_ptr to indicate buffer consumed
//   5. Check for completion (all buffers transmitted)
//
// CPU responsibilities (in poll()):
//   - Pre-compute next DMA buffer when ring has space
//   - Update ring_write_ptr after pre-computation
//   - Detect stream_complete flag for transmission end
__attribute__((optimize("O3"))) bool FL_IRAM
ChannelEnginePARLIOImpl::txDoneCallback(parlio_tx_unit_handle_t tx_unit,
                                        const void *edata, void *user_ctx) {
    (void)edata;

    auto *self = static_cast<ChannelEnginePARLIOImpl *>(user_ctx);
    if (!self || !self->mState.mIsrContext) {
        return false;
    }

    // Phase 0: Access ISR state via cache-aligned ParlioIsrContext struct
    ParlioIsrContext *ctx = self->mState.mIsrContext;

    // Step 1: Increment completion counter (signals CPU that one buffer
    // finished)
    ctx->buffers_completed++;
    ctx->isr_count++;

    // Step 2: Check if all buffers have been submitted - mark transmission
    // complete
    if (ctx->buffers_completed >= ctx->buffers_total) {
        ctx->stream_complete = true;
        ctx->transmitting = false;
        return false; // No high-priority task woken
    }

    // More buffers need to be submitted - check if next buffer is ready
    volatile size_t read_ptr = ctx->ring_read_ptr;
    volatile size_t write_ptr = ctx->ring_write_ptr;

    // Ring underflow - CPU didn't generate buffer fast enough
    if (read_ptr == write_ptr) {
        ctx->ring_error = true;
        return false;
    }

    // Next buffer is ready - submit it to hardware
    size_t buffer_idx = read_ptr % ctx->ring_size;
    uint8_t *buffer_ptr = self->mState.ring_buffers[buffer_idx].get();
    size_t buffer_size = self->mState.ring_buffer_sizes[buffer_idx];

    // Invalid buffer - set error flag
    if (!buffer_ptr || buffer_size == 0) {
        ctx->ring_error = true;
        return false;
    }

    // Submit buffer to hardware
    // Phase 1 - Iteration 2: Use consistent idle_value = 0x0000 (LOW) for all
    // buffers WS2812B protocol requires LOW state between data frames
    // Alternating idle_value creates spurious transitions between chunks
    // ITERATION 5 - Experiment 1-2: idle_value = HIGH FAILED (100% error rate)
    // ITERATION 5 - Experiment 3: Revert to idle_value = LOW, test with
    // sample_edge = NEG
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

    return false; // No high-priority task woken
}

//=============================================================================
// Phase 0: Ring Buffer Generation (CPU Thread)
//=============================================================================

/// @brief Populate a DMA buffer with waveform data for a byte range
///
/// Two-stage processing model (repeated for each byte position):
///   Stage 1: Generate wave8bytes for ALL lanes → staging buffer (waveform_expansion_buffer)
///   Stage 2: Transpose staging buffer → DMA output buffer (bit-packed PARLIO format)
///
/// @param outputBuffer DMA buffer to populate (pre-allocated and pre-zeroed)
/// @param outputBufferCapacity Maximum size of output buffer
/// @param startByte Starting byte offset in source data
/// @param byteCount Number of bytes to process
/// @param outputBytesWritten [out] Number of bytes written to output buffer
/// @return true on success, false on error (buffer overflow, etc.)
bool ChannelEnginePARLIOImpl::populateDmaBuffer(uint8_t* outputBuffer,
                                                size_t outputBufferCapacity,
                                                size_t startByte,
                                                size_t byteCount,
                                                size_t& outputBytesWritten) {
    // Staging buffer for wave8 output before transposition
    // Holds wave8bytes for all lanes (data_width × 8 bytes)
    // Each lane produces Wave8Byte (8 bytes) for each input byte
    uint8_t* laneWaveforms = mState.waveform_expansion_buffer.get();
    constexpr size_t bytes_per_lane = sizeof(Wave8Byte); // 8 bytes per input byte

    size_t outputIdx = 0;
    size_t byteOffset = 0;

    // Two-stage architecture: Process one byte position at a time
    // Stage 1: Generate wave8bytes for ALL lanes → staging buffer
    // Stage 2: Transpose staging buffer → DMA output buffer
    while (byteOffset < byteCount) {
        // Calculate block size for buffer space check
        size_t blockSize = (mState.data_width <= 8) ? 8 : 16;

        // Check if enough space for this block
        if (outputIdx + blockSize > outputBufferCapacity) {
            // Buffer overflow - return error immediately
            outputBytesWritten = outputIdx;
            return false;
        }

        // ═══════════════════════════════════════════════════════════════
        // STAGE 1: Generate wave8bytes for ALL lanes into staging buffer
        // ═══════════════════════════════════════════════════════════════
        for (size_t lane = 0; lane < mState.data_width; lane++) {
            uint8_t* laneWaveform = laneWaveforms + (lane * bytes_per_lane);

            if (lane < mState.actual_channels) {
                // Real channel - expand using wave8
                const uint8_t* laneData = mState.scratch_padded_buffer.data() +
                                          (lane * mState.lane_stride);
                uint8_t byte = laneData[startByte + byteOffset];

                // wave8() outputs Wave8Byte (8 bytes) in bit-packed format
                // Cast pointer to array reference for wave8 API
                uint8_t (*wave8Array)[8] = reinterpret_cast<uint8_t (*)[8]>(laneWaveform);
                fl::wave8(byte, mState.wave8_lut, *wave8Array);
            } else {
                // Dummy lane - zero waveform (keeps GPIO LOW, 0x00 bytes)
                fl::memset(laneWaveform, 0x00, bytes_per_lane);
            }
        }

        // ═══════════════════════════════════════════════════════════════
        // STAGE 2: Transpose staging buffer → DMA output buffer
        // ═══════════════════════════════════════════════════════════════
        // Transpose wave8bytes from all lanes (laneWaveforms staging buffer)
        // into bit-packed format for PARLIO hardware transmission
        size_t bytesWritten = fl::transpose_wave8byte_parlio(
            laneWaveforms,        // Input: staging buffer (all lanes' wave8bytes)
            mState.data_width,    // Number of lanes (1-16)
            outputBuffer + outputIdx);  // Output: DMA buffer

        outputIdx += bytesWritten;
        byteOffset++;
    }

    outputBytesWritten = outputIdx;
    return true;
}

//=============================================================================
// Ring Buffer Management - Incremental Population Architecture
//=============================================================================

bool ChannelEnginePARLIOImpl::has_ring_space() const {
    if (!mState.mIsrContext) {
        return false;
    }

    // Calculate how many buffers are currently in the ring (filled but not consumed)
    volatile size_t write_ptr = mState.mIsrContext->ring_write_ptr;
    volatile size_t read_ptr = mState.mIsrContext->ring_read_ptr;
    size_t filled = write_ptr - read_ptr;

    // Ring has space if filled count is less than total ring size
    return filled < PARLIO_RING_BUFFER_COUNT;
}

bool ChannelEnginePARLIOImpl::generateRingBuffer() {
    // One-time ring buffer allocation and initialization
    // Called once during initializeIfNeeded(), NOT per transmission
    // Buffers remain allocated, only POPULATED on-demand during transmission

    // Clear any existing ring buffers
    mState.ring_buffers.clear();
    mState.ring_buffer_sizes.clear();

    // Allocate all ring buffers with DMA capability
    for (size_t i = 0; i < PARLIO_RING_BUFFER_COUNT; i++) {
        fl::unique_ptr<uint8_t[], HeapCapsDeleter> buffer(
            static_cast<uint8_t *>(heap_caps_malloc(
                mState.ring_buffer_capacity, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL)));

        if (!buffer) {
            FL_WARN("PARLIO: Failed to allocate ring buffer "
                    << i << "/" << PARLIO_RING_BUFFER_COUNT << " (requested "
                    << mState.ring_buffer_capacity << " bytes)");
            // Clean up already allocated ring buffers (automatic via unique_ptr destructors)
            mState.ring_buffers.clear();
            mState.ring_buffer_sizes.clear();
            return false;
        }

        // Zero-initialize buffer to prevent garbage data
        fl::memset(buffer.get(), 0x00, mState.ring_buffer_capacity);

        mState.ring_buffers.push_back(fl::move(buffer));
        mState.ring_buffer_sizes.push_back(0); // Will be set during population
    }

    return true;
}

bool ChannelEnginePARLIOImpl::populateNextDMABuffer() {
    // Incremental buffer population - called repeatedly to fill ring buffers
    // Returns true if more buffers need to be populated, false if all done

    if (!mState.mIsrContext) {
        return false;
    }

    // Check if we've populated all buffers
    if (mState.buffers_populated >= mState.mIsrContext->buffers_total) {
        return false; // All done
    }

    // Check if more data to process
    if (mState.next_byte_offset >= mState.mIsrContext->total_bytes) {
        return false; // No more source data
    }

    // Calculate ring buffer index (wraps around)
    size_t ring_index = mState.next_buffer_to_populate % PARLIO_RING_BUFFER_COUNT;

    // Get ring buffer pointer
    uint8_t *outputBuffer = mState.ring_buffers[ring_index].get();
    if (!outputBuffer) {
        FL_WARN("PARLIO: Ring buffer " << ring_index << " not allocated");
        mState.error_occurred = true;
        return false;
    }

    // Calculate byte range for this buffer
    size_t bytes_remaining = mState.mIsrContext->total_bytes - mState.next_byte_offset;
    size_t bytes_per_buffer = (mState.mIsrContext->total_bytes + mState.mIsrContext->buffers_total - 1) / mState.mIsrContext->buffers_total;
    size_t byte_count = (bytes_remaining < bytes_per_buffer) ? bytes_remaining : bytes_per_buffer;

    // Zero output buffer to prevent garbage data from previous use
    fl::memset(outputBuffer, 0x00, mState.ring_buffer_capacity);

    // Generate waveform data using helper function
    size_t outputBytesWritten = 0;
    if (!populateDmaBuffer(outputBuffer, mState.ring_buffer_capacity,
                          mState.next_byte_offset, byte_count, outputBytesWritten)) {
        FL_WARN("PARLIO: Ring buffer overflow at ring_index=" << ring_index);
        mState.error_occurred = true;
        return false;
    }

    // Store actual size of this buffer
    mState.ring_buffer_sizes[ring_index] = outputBytesWritten;

    // DEBUG: Copy DMA buffer data to debug deque for validation
    if (mState.mIsrContext) {
        // Append this buffer's data to the debug deque (push_back each byte)
        for (size_t i = 0; i < outputBytesWritten; i++) {
            mState.mIsrContext->mDebugDmaOutput.push_back(outputBuffer[i]);
        }
    }

    // Update state for next buffer
    mState.next_byte_offset += byte_count;
    mState.next_buffer_to_populate++;
    mState.buffers_populated++;

    // Signal ISR that buffer is ready (increment write pointer)
    mState.mIsrContext->ring_write_ptr++;

    // Return true if more buffers need to be populated
    return mState.buffers_populated < mState.mIsrContext->buffers_total;
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
    // Read stream_complete first (volatile field ensures fresh read from
    // memory)
    if (mState.mIsrContext->stream_complete) {
        // Execute memory barrier to synchronize all ISR writes
        // This ensures all non-volatile ISR data (isr_count, bytes_transmitted,
        // etc.) is visible
        PARLIO_MEMORY_BARRIER();

        // Clear completion flags BEFORE cleanup operations (volatile
        // assignment) This prevents race condition where new transmission could
        // start during cleanup Previous code cleared flags AFTER cleanup,
        // allowing state corruption
        mState.mIsrContext->transmitting = false;
        mState.mIsrContext->stream_complete = false;

        // Wait for final chunk to complete
        esp_err_t err = parlio_tx_unit_wait_all_done(mState.tx_unit, 0);

        if (err == ESP_OK) {
            // FIX C5-1 PARTIAL REVERT: GPIO reset loop IS needed (Iteration 21
            // fix was correct) Analysis showed Patterns B-D fail without GPIO
            // stabilization between transmissions PARLIO idle_value alone is
            // insufficient to prevent GPIO glitches on subsequent starts
            // OPTIMIZATION: Reduced delay from 1ms → 100μs (10x faster, still
            // prevents glitches)

            // Disable PARLIO to reset peripheral state
            err = parlio_tx_unit_disable(mState.tx_unit);
            if (err != ESP_OK) {
                FL_WARN("PARLIO: Failed to disable TX unit after transmission: "
                        << err);
            }

            // Force all GPIO pins to LOW state
            for (size_t i = 0; i < mState.actual_channels; i++) {
                int pin = mState.pins[i];
                if (pin >= 0) {
                    gpio_set_level(static_cast<gpio_num_t>(pin), 0);
                }
            }

            // Reduced delay: 100μs instead of 1ms (10x faster, 2% overhead
            // instead of 20%)
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

    // NEW ARCHITECTURE: Incremental ring buffer refill during transmission
    // As ISR consumes buffers (ring_read_ptr advances), populate new buffers
    // to keep ring full and prevent underflow
    while (has_ring_space() && populateNextDMABuffer()) {
        // Continue populating buffers as long as:
        // 1. Ring has space (write_ptr - read_ptr < PARLIO_RING_BUFFER_COUNT)
        // 2. More buffers need to be populated (returns false when all done)
    }

    // Transmission in progress - wait for ISR to mark completion
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
            FL_WARN(
                "  This pin is either forbidden (SPI flash, strapping, etc.)");
            FL_WARN("  or not a valid output pin for this ESP32 variant.");
            FL_WARN("  See FASTLED_UNUSABLE_PIN_MASK in fastpin_esp32.h for "
                    "details.");
            return;
        }

        mState.pins.push_back(pin);
    }

    // Fill remaining pins with -1 for dummy lanes (unused)
    for (size_t i = channel_count; i < mState.data_width; i++) {
        mState.pins.push_back(-1);
    }

    // FIX C1-3: Extract timing configuration BEFORE initializeIfNeeded()
    // This ensures waveform generation has correct timing parameters from the
    // start Previously, timing was set AFTER init, causing first frame to use
    // uninitialized values (0,0,0)
    const ChipsetTimingConfig &timing = channelData[0]->getTiming();
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

    size_t totalBytes = maxChannelSize;
    // Prepare single segmented scratch buffer for all N lanes
    // Layout: [lane0_data][lane1_data]...[laneN_data]
    // Each lane is maxChannelSize bytes (padded to same size)
    // This is regular heap (non-DMA) - only output waveform buffers need DMA
    // capability

    size_t totalBufferSize = channelData.size() * maxChannelSize;
    mState.scratch_padded_buffer.resize(totalBufferSize);

    // FIX H3-4: Removed redundant scratch buffer zero-fill (1-2ms savings per
    // frame) Subsequent loop (lines 862-882) overwrites all bytes via memcpy +
    // right-padding memset Pattern D corruption (Iteration 22) was fixed by
    // right-padding logic, not this memset

    mState.lane_stride = maxChannelSize;
    mState.num_lanes = channelData.size();
    // Phase 5: Also set num_lanes in ISR context (ISR needs this for
    // bytes_transmitted calculation)
    mState.mIsrContext->num_lanes = channelData.size();

    // Write all channels to segmented scratch buffer with stride-based layout
    for (size_t i = 0; i < channelData.size(); i++) {
        size_t dataSize = channelData[i]->getSize();
        uint8_t *laneDst =
            mState.scratch_padded_buffer.data() + (i * maxChannelSize);
        fl::span<uint8_t> dst(laneDst, maxChannelSize);

        // PARLIO FIX: Never use left-padding for multi-lane configurations
        // All lanes MUST have same number of LEDs, so right-pad with zeros if
        // needed
        const auto &srcData = channelData[i]->getData();
        if (dataSize < maxChannelSize) {
            // Lane needs padding - use RIGHT-padding (zeros at end) instead of
            // left-padding
            fl::memcpy(laneDst, srcData.data(),
                       dataSize); // Copy actual data first
            fl::memset(laneDst + dataSize, 0,
                       maxChannelSize - dataSize); // Zero-fill remainder
        } else {
            // No padding needed - direct copy
            fl::memcpy(laneDst, srcData.data(), maxChannelSize);
        }
    }

    // Phase 0: Ring buffer streaming architecture with CPU-based waveform
    // generation Initialize IsrContext state for ring buffer streaming
    mState.mIsrContext->total_bytes = totalBytes;
    mState.mIsrContext->current_byte = 0;
    mState.mIsrContext->stream_complete = false;
    mState.error_occurred = false;
    mState.mIsrContext->transmitting =
        false; // Will be set to true after first buffer submitted

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

    // ITERATION 1: OLD buffer allocation (BROKEN - splits small strips into
    // multiple buffers) size_t leds_per_buffer = (numLeds +
    // PARLIO_RING_BUFFER_COUNT - 1) / PARLIO_RING_BUFFER_COUNT; size_t
    // total_buffers = (numLeds + leds_per_buffer - 1) / leds_per_buffer;

    // ITERATION 2: NEW buffer allocation - single-buffer mode for Phase 0
    // Phase 0 requirement: ALL bytes must fit in ONE buffer to avoid DMA gaps
    // For small data (fits in ring buffer capacity), use single buffer mode
    // For large data (exceeds capacity), use multi-buffer ring mode
    size_t single_buffer_max_bytes = mState.ring_buffer_capacity;
    size_t bytes_per_buffer;
    size_t total_buffers;

    if (totalBytes <= single_buffer_max_bytes) {
        // ITERATION 2: Phase 0 path - all bytes fit in one buffer (NO DMA gaps)
        bytes_per_buffer = totalBytes;
        total_buffers = 1;
        FL_WARN("PARLIO: Single-buffer mode activated ("
                << totalBytes << " bytes fit in one buffer)");
    } else {
        // ITERATION 2: Phase 1+ path - multi-buffer streaming with DMA gaps
        bytes_per_buffer = (totalBytes + PARLIO_RING_BUFFER_COUNT - 1) /
                           PARLIO_RING_BUFFER_COUNT;
        total_buffers = (totalBytes + bytes_per_buffer - 1) / bytes_per_buffer;
        FL_WARN("PARLIO: Multi-buffer mode activated ("
                << totalBytes << " bytes across " << total_buffers
                << " buffers, " << bytes_per_buffer << " bytes per buffer)");
    }

    mState.mIsrContext->buffers_submitted = 0;
    mState.mIsrContext->buffers_completed = 0;
    mState.mIsrContext->buffers_total = total_buffers;

    // NEW ARCHITECTURE: Incremental ring buffer population
    // Ring buffers already allocated during initializeIfNeeded()
    // Now populate them incrementally with waveform data

    // Step 1: Initialize population state
    mState.next_buffer_to_populate = 0;
    mState.next_byte_offset = 0;
    mState.buffers_populated = 0;

    FL_WARN("PARLIO: Pre-populating ring buffers (up to "
            << PARLIO_RING_BUFFER_COUNT << " buffers)");

    // Step 2: Pre-populate ring buffers (fill as many as ring capacity allows)
    // This ensures ISR has buffers ready when transmission starts
    // Remaining buffers will be populated incrementally during poll()
    while (has_ring_space() && populateNextDMABuffer()) {
        // Loop until ring is full or all buffers populated
        FL_WARN("PARLIO: Pre-populated buffer "
                << (mState.buffers_populated - 1) << "/" << total_buffers
                << " (ring_write_ptr=" << mState.mIsrContext->ring_write_ptr << ")");
    }

    FL_WARN("PARLIO: Pre-populated " << mState.buffers_populated << "/"
                                     << total_buffers << " buffers");

    // Verify at least one buffer was populated
    if (mState.buffers_populated == 0) {
        FL_WARN("PARLIO: No buffers populated - cannot start transmission");
        mState.error_occurred = true;
        return;
    }

    // Phase 0: Submit ONLY first buffer from CPU thread
    // ISR will submit all subsequent buffers as they complete
    size_t first_buffer_idx = 0;
    size_t first_buffer_size = mState.ring_buffer_sizes[first_buffer_idx];

    // ITERATION 5 - Experiment 1-2: idle_value = HIGH FAILED (100% error rate)
    // ITERATION 5 - Experiment 3: Revert to idle_value = LOW, test with
    // sample_edge = NEG
    parlio_transmit_config_t tx_config = {};
    tx_config.idle_value = 0x0000; // Start with LOW (reverted)
    // tx_config.idle_value = 0xFFFF; // ITERATION 5: HIGH failed (disabled)

    esp_err_t err = parlio_tx_unit_transmit(
        mState.tx_unit, mState.ring_buffers[first_buffer_idx].get(),
        first_buffer_size * 8, &tx_config);

    if (err != ESP_OK) {
        FL_WARN("PARLIO: Failed to submit first buffer: " << err);
        mState.error_occurred = true;
        return;
    }

    FL_WARN("PARLIO: Submitted first buffer (" << first_buffer_size
                                               << " bytes)");

    // Update read pointer (first buffer submitted, ISR will submit rest)
    mState.mIsrContext->ring_read_ptr = 1; // ISR starts from buffer 1

    // Mark transmission started and buffer accounting
    mState.mIsrContext->transmitting = true;
    mState.mIsrContext->buffers_submitted =
        1; // Only first buffer submitted by CPU

    FL_WARN("PARLIO: Transmission started - ISR will handle remaining "
            << (total_buffers - 1) << " buffers");
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
        ParlioIsrContext::setInstance(
            mState.mIsrContext); // Set singleton reference
    }

    // Version compatibility check for ESP32-C6
    // PARLIO driver has a bug in IDF versions < 5.5 on ESP32-C6
#if defined(CONFIG_IDF_TARGET_ESP32C6)
#if ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(5, 5, 0)
    FL_WARN(
        "PARLIO: ESP32-C6 requires ESP-IDF 5.5.0 or later (current version: "
        << ESP_IDF_VERSION_MAJOR << "." << ESP_IDF_VERSION_MINOR << "."
        << ESP_IDF_VERSION_PATCH
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
        FL_WARN(
            "PARLIO: Requested data width "
            << mState.data_width << " bits exceeds hardware maximum "
            << SOC_PARLIO_TX_UNIT_MAX_DATA_WIDTH << " bits on this chip. "
            << "This chip does not support " << mState.data_width
            << "-bit mode. "
            << "Maximum supported: " << SOC_PARLIO_TX_UNIT_MAX_DATA_WIDTH
            << " bits. "
            << "Reduce channel count or use a chip with wider PARLIO support.");
        mInitialized = false;
        return;
    }
#else
    // Older ESP-IDF without SOC capability macros - perform basic validation
    // This fallback ensures compatibility with older IDF versions while still
    // providing some level of validation
    if (mState.data_width > 16) {
        FL_WARN("PARLIO: Requested data width "
                << mState.data_width
                << " bits exceeds assumed maximum (16 bits). "
                << "Hardware capability unknown - update ESP-IDF for proper "
                   "detection. "
                << "Proceeding with caution...");
        // Don't fail - older IDF may still work, just warn
    }
#endif

    // Step 1: Build wave8 expansion LUT from timing configuration
    // Timing parameters are extracted from the first channel in
    // beginTransmission() and stored in mState.timing_t1_ns,
    // mState.timing_t2_ns, mState.timing_t3_ns
    //
    // For WS2812B-V5 (typical values):
    // - T1: 300ns (T0H - initial HIGH time)
    // - T2: 200ns (T1H-T0H - additional HIGH time for bit 1)
    // - T3: 500ns (T0L - LOW tail duration)
    //
    // wave8 normalizes timing to 8 pulses per bit (fixed 8:1 expansion)
    ChipsetTiming timing;
    timing.T1 = mState.timing_t1_ns;
    timing.T2 = mState.timing_t2_ns;
    timing.T3 = mState.timing_t3_ns;
    timing.RESET = 0; // Not used for waveform generation
    timing.name = "PARLIO";

    mState.wave8_lut = buildWave8ExpansionLUT(timing);
    // Step 2: Calculate width-adaptive streaming chunk size
    // wave8 uses fixed 8 pulses per bit (8:1 expansion)
    // After bit-packing transpose:
    // - For data_width <= 8: outputs (8 pulses × 8 bits / (8/data_width)) bytes
    // per input byte
    // - For data_width = 1: outputs 8 bytes per input byte (after transpose)
    // - For data_width = 8: outputs 8 bytes per input byte (after transpose)
    // - Each input byte → 8 output bytes (after transpose, for data_width <= 8)
    constexpr size_t pulses_per_bit = 8; // Fixed for wave8
    size_t bytes_per_input_byte;
    if (mState.data_width <= 8) {
        // After transpose: 8 pulses × 8 bits / (8/data_width) ticks per byte
        // For data_width=1: 8*8 / 8 = 8 bytes per input byte
        bytes_per_input_byte = 8 * pulses_per_bit / (8 / mState.data_width);
    } else {
        // For 16-bit width: 2 bytes per pulse tick
        bytes_per_input_byte = 8 * pulses_per_bit * 2;
    }
    // PHASE 1 OPTION E: Increase buffer size to reduce chunk count below ISR
    // callback limit Root cause: ESP-IDF PARLIO driver stops invoking ISR
    // callbacks after 4 transmissions Solution: Use larger buffers (18KB) to
    // process more bytes per chunk Previous: 2KB buffer → small chunks → many
    // chunks (exceeds 4-callback limit) New: 18KB buffer → larger chunks →
    // fewer chunks (works within ISR callback limit)
    constexpr size_t target_buffer_size = 18432; // 18KB per buffer
    mState.bytes_per_chunk = target_buffer_size / bytes_per_input_byte;

    // Clamp to reasonable range (in input bytes)
    if (mState.bytes_per_chunk < 10)
        mState.bytes_per_chunk = 10;
    if (mState.bytes_per_chunk > 5000)
        mState.bytes_per_chunk = 5000;

    // Step 3: Validate GPIO pins were provided by beginTransmission()
    // Pins are extracted from ChannelData and validated before initialization
    if (mState.pins.size() != mState.data_width) {
        FL_WARN("PARLIO: Pin configuration error - expected "
                << mState.data_width << " pins, got " << mState.pins.size());
        FL_WARN(
            "  Pins must be provided via FastLED.addLeds<WS2812, PIN>() API");
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
    // Phase 1 - Iteration 2: Set trans_queue_depth to 2 (minimum for ISR
    // streaming) PARLIO has 3 internal queues (READY, PROGRESS, COMPLETE) per
    // README.md line 101 With depth=1, ISR callback can't queue next buffer
    // (completed transaction still in COMPLETE queue) With depth=2, allows: 1
    // in PROGRESS + 1 buffer for ISR to queue → prevents watchdog timeout
    // Testing showed depth=1 causes watchdog timeout; depth=2 resolves it
    config.trans_queue_depth = 2;
    // Phase 3 - Iteration 19: ESP-IDF PARLIO hardware limit is 65535 BITS per
    // transmission This is a hard hardware limit that cannot be exceeded For
    // transmissions larger than this, we must chunk into multiple calls Set
    // max_transfer_size to hardware maximum (must be < 65535, so use 65534)
    config.max_transfer_size =
        65534; // Hardware maximum (65535 bits - 1 for safety)
    config.bit_pack_order =
        PARLIO_BIT_PACK_ORDER_LSB; // Lane 0 = bit 0, transmit bit 0 first
    // ITERATION 5 - Experiment 2-3: sample_edge = NEG made things WORSE (39
    // corrupt LEDs vs 24) ITERATION 5 - Reverting to POS (best configuration
    // found in Iteration 1)
    config.sample_edge =
        PARLIO_SAMPLE_EDGE_POS; // ITERATION 1: Match ESP-IDF example (was NEG)
    // config.sample_edge =
    //     PARLIO_SAMPLE_EDGE_NEG; // ITERATION 5: Experiment 2-3 - NEG failed
    //     (disabled)

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

    // Step 6: Register ISR callback for streaming (MUST be done BEFORE
    // enabling) CRITICAL: ESP-IDF requires callbacks to be registered before
    // parlio_tx_unit_enable() Registering after enable causes callbacks to
    // never fire
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

    // wave8 uses fixed 8:1 expansion, output is bit-packed
    // After transpose, each input byte becomes:
    // - data_width=1: 8 bytes output (64 pulses / 8 ticks per byte)
    // - data_width=8: 8 bytes output (64 pulses / 8 ticks per byte, bit-packed
    // across lanes)
    // - data_width=16: 16 bytes output (64 pulses × 2 bytes per pulse)
    size_t bytes_per_input_byte_output;
    if (mState.data_width <= 8) {
        // 8 bytes per input byte after transpose (8 pulses bit-packed)
        bytes_per_input_byte_output = 8;
    } else {
        // 16 bytes per input byte for 16-bit width (16 pulses, 2 bytes per
        // pulse)
        bytes_per_input_byte_output = 16;
    }
    size_t bufferSize = mState.bytes_per_chunk * bytes_per_input_byte_output;

    // Buffer size calculation:
    // - bytes_per_chunk: number of input bytes to process per chunk
    // - bytes_per_input_byte_output: output bytes generated per input byte
    // (after wave8 + transpose)
    // - bufferSize: total DMA buffer size for chunk
    //
    // For data_width=1: bytes_per_input_byte_output = 8
    // For data_width=8: bytes_per_input_byte_output = 8 (bit-packed across
    // lanes) For data_width=16: bytes_per_input_byte_output = 16

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

    // Calculate ring buffer capacity (used later during beginTransmission)
    // Target: ~125 input bytes per buffer → 125 * 8 = 1000 output bytes (for data_width <= 8)
    size_t ring_buffer_input_byte_count = 125; // Input bytes per ring buffer

    // wave8 uses fixed 8:1 expansion with bit-packing
    // After transpose: each input byte → output bytes (depends on data_width)
    size_t bytes_per_input_byte_ring;
    if (mState.data_width <= 8) {
        // 8 bytes output per input byte (8 pulses bit-packed)
        bytes_per_input_byte_ring = 8;
    } else if (mState.data_width == 16) {
        // For 16-bit width: 16 bytes output per input byte
        bytes_per_input_byte_ring = 16;
    } else {
        bytes_per_input_byte_ring = 8; // Fallback
    }

    mState.ring_buffer_capacity =
        ring_buffer_input_byte_count * bytes_per_input_byte_ring;

    // Step 8: Allocate ring buffers upfront (all memory allocated during init)
    // Buffers will be populated on-demand during transmission
    if (!generateRingBuffer()) {
        FL_WARN("PARLIO: Failed to allocate ring buffers during initialization");

        // Clean up old buffers
        mState.buffer_a.reset();
        mState.buffer_b.reset();

        // Clean up TX unit
        esp_err_t disable_err = parlio_tx_unit_disable(mState.tx_unit);
        if (disable_err != ESP_OK) {
            FL_WARN("PARLIO: Failed to disable TX unit during ring buffer cleanup: "
                    << disable_err);
        }
        esp_err_t del_err = parlio_del_tx_unit(mState.tx_unit);
        if (del_err != ESP_OK) {
            FL_WARN("PARLIO: Failed to delete TX unit during ring buffer cleanup: "
                    << del_err);
        }
        mState.tx_unit = nullptr;
        mInitialized = false;
        return;
    }

    // Step 9: Allocate waveform expansion buffer for wave8 output
    // wave8 produces Wave8Byte (8 bytes) for each input byte
    // Buffer size: 16 lanes (max) × 8 bytes per lane = 128 bytes
    // CRITICAL REQUIREMENTS:
    // 1. MUST be heap-allocated (NOT stack) - used in ISR context
    // 2. Does NOT need to be DMA-capable - only used temporarily for waveform
    // expansion
    // 3. buffer_a and buffer_b (allocated above) MUST be DMA-capable for
    // PARLIO/GDMA hardware
    // 4. This buffer is managed by mState and freed in the destructor
    const size_t bytes_per_lane = sizeof(Wave8Byte); // 8 bytes per input byte
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
        mState.mIsrContext->current_byte = 0;
        mState.mIsrContext->total_bytes = 0;
    }
    mState.error_occurred = false;

    mInitialized = true;
}

//=============================================================================
// Polymorphic Wrapper Class Implementation
//=============================================================================

ChannelEnginePARLIO::ChannelEnginePARLIO() : mEngine(), mCurrentDataWidth(0) {}

ChannelEnginePARLIO::~ChannelEnginePARLIO() {
    // fl::unique_ptr automatically deletes mEngine when it goes out of scope
    // (RAII)
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
    auto *ctx = ParlioIsrContext::getInstance();
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
    // After wave8 expansion + transpose: each input byte → 8 output bytes (for
    // data_width <= 8)
    size_t bytes_per_input_byte_output =
        8; // Simplified estimate (assumes data_width <= 8)
    size_t bytes_per_buffer =
        (ctx->buffers_total > 0) ? (ctx->total_bytes / ctx->buffers_total) : 0;
    if (ctx->buffers_total > 0) {
        metrics.bytes_total = ctx->total_bytes * bytes_per_input_byte_output;
        metrics.bytes_transmitted = ctx->buffers_completed * bytes_per_buffer *
                                    bytes_per_input_byte_output;
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
