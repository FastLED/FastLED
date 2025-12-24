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
#include "fl/stl/algorithm.h"
#include "fl/stl/assert.h"
#include "fl/stl/time.h"
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

// WS2812B PARLIO clock frequency
// - 8.0 MHz produces 125ns per tick (matches wave8 8-pulse expansion)
// - Each LED bit = 8 clock ticks = 1.0μs total
// - Divides from PLL_F160M on ESP32-P4 (160/20) or PLL_F240M on ESP32-C6 (240/30)
static constexpr uint32_t PARLIO_CLOCK_FREQ_HZ = 8000000; // 8.0 MHz

constexpr size_t MAX_LEDS_PER_CHANNEL = 300; // Support up to 300 LEDs per channel (configurable)

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
// Buffer Size Calculator - Unified DMA Buffer Size Calculations
//=============================================================================

/// @brief Unified calculator for PARLIO buffer sizes
///
/// Consolidates all buffer size calculations into a single, tested utility.
/// Wave8 expands each input byte to 64 pulses (8 bits × 8 pulses per bit).
/// Transposition packs pulses into bytes based on data_width.
struct ParlioBufferCalculator {
    size_t mDataWdith;

    /// @brief Calculate output bytes per input byte after wave8 + transpose
    /// @return Output bytes per input byte (8 for width≤8, 128 for width=16)
    size_t outputBytesPerInputByte() const {
        if (mDataWdith <= 8) {
            // Bit-packed: 64 pulses / (8 / mDataWdith) ticks per byte = 8 bytes
            return 8;
        } else if (mDataWdith == 16) {
            // 16-bit mode: 64 pulses × 2 bytes per pulse = 128 bytes
            return 128;
        }
        return 8; // Fallback
    }

    /// @brief Calculate DMA buffer size for given input bytes
    /// @param inputBytes Number of input bytes to transmit
    /// @return Total DMA buffer size in bytes
    size_t dmaBufferSize(size_t inputBytes) const {
        return inputBytes * outputBytesPerInputByte();
    }

    /// @brief Calculate transpose output block size for populateDmaBuffer
    /// @return Block size in bytes for transpose operation
    size_t transposeBlockSize() const {
        if (mDataWdith <= 8) {
            size_t ticksPerByte = 8 / mDataWdith;
            size_t pulsesPerByte = 64;
            return (pulsesPerByte + ticksPerByte - 1) / ticksPerByte;
        } else if (mDataWdith == 16) {
            return 128; // 64 pulses × 2 bytes per pulse
        }
        return 8; // Fallback
    }

    /// @brief Calculate optimal ring buffer capacity based on LED frame boundaries
    /// @param maxLedsPerChannel Maximum LEDs per channel (strip size)
    /// @param numRingBuffers Number of ring buffers (default: 3)
    /// @return DMA buffer capacity in bytes, aligned to LED boundaries
    ///
    /// Algorithm:
    /// 1. Calculate LEDs per buffer: maxLedsPerChannel / numRingBuffers
    /// 2. Convert to input bytes: LEDs × 3 bytes/LED × mDataWdith (multi-lane)
    /// 3. Apply wave8 expansion (8:1 ratio): input_bytes × outputBytesPerInputByte()
    /// 4. Result is DMA buffer capacity per ring buffer
    ///
    /// Example (3000 LEDs, 1 lane, 3 ring buffers):
    /// - LEDs per buffer: 3000 / 3 = 1000 LEDs
    /// - Input bytes per buffer: 1000 × 3 × 1 = 3000 bytes
    /// - DMA bytes per buffer: 3000 × 8 = 24000 bytes
    size_t calculateRingBufferCapacity(size_t maxLedsPerChannel, size_t numRingBuffers = 3) const {
        // Step 1: Calculate LEDs per buffer (divide total LEDs by number of buffers)
        size_t ledsPerBuffer = (maxLedsPerChannel + numRingBuffers - 1) / numRingBuffers;

        // Step 2: Calculate input bytes per buffer
        // - 3 bytes per LED (RGB)
        // - Multiply by mDataWdith for multi-lane (each lane gets same LED count)
        size_t inputBytesPerBuffer = ledsPerBuffer * 3 * mDataWdith;

        // Step 3: Apply wave8 expansion (8:1 ratio for ≤8-bit width, 128:1 for 16-bit)
        size_t dmaBufferCapacity = dmaBufferSize(inputBytesPerBuffer);

        return dmaBufferCapacity;
    }
};

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
    if (mState.mTxUnit != nullptr) {
        // Wait for any pending transmissions (with timeout)
        esp_err_t err =
            parlio_tx_unit_wait_all_done(mState.mTxUnit, pdMS_TO_TICKS(1000));
        if (err != ESP_OK) {
            FL_WARN("PARLIO: Wait for transmission timeout during cleanup: "
                    << err);
        }

        // Disable TX unit
        err = parlio_tx_unit_disable(mState.mTxUnit);
        if (err != ESP_OK) {
            FL_WARN("PARLIO: Failed to disable TX unit: " << err);
        }

        // Delete TX unit
        err = parlio_del_tx_unit(mState.mTxUnit);
        if (err != ESP_OK) {
            FL_WARN("PARLIO: Failed to delete TX unit: " << err);
        }

        mState.mTxUnit = nullptr;
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
    mState.mPins.clear();
    mState.mScratchPaddedBuffer.clear();
    mState.mNumLanes = 0;
    mState.mLaneStride = 0;
    // Phase 4: Clear IsrContext state (if allocated)
    if (mState.mIsrContext) {
        mState.mIsrContext->mTransmitting = false;
        mState.mIsrContext->mStreamComplete = false;
    }
    mState.mErrorOccurred = false;
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
FL_OPTIMIZE_FUNCTION bool FL_IRAM
ChannelEnginePARLIOImpl::txDoneCallback(parlio_tx_unit_handle_t tx_unit,
                                        const void *edata, void *user_ctx) {
    (void)edata;

    auto *self = static_cast<ChannelEnginePARLIOImpl *>(user_ctx);
    if (!self || !self->mState.mIsrContext) {
        return false;
    }

    // Access ISR state via cache-aligned ParlioIsrContext struct
    ParlioIsrContext *ctx = self->mState.mIsrContext;

    // Increment ISR call counter
    ctx->mIsrCount++;

    // Account for bytes from the buffer that just completed transmission
    // The buffer that completed is the one BEFORE the current read_idx
    // (CPU or previous ISR call advanced read_idx after submitting)
    volatile size_t read_idx = ctx->mRingReadIdx;
    size_t completed_buffer_idx = (read_idx + PARLIO_RING_BUFFER_COUNT - 1) % PARLIO_RING_BUFFER_COUNT;

    // Track transmitted bytes (using input byte count, not expanded DMA bytes)
    // Calculate input bytes from DMA buffer size
    ParlioBufferCalculator calc{self->mState.mDataWidth};
    size_t dma_bytes = self->mState.mRingBufferSizes[completed_buffer_idx];
    size_t input_bytes = dma_bytes / calc.outputBytesPerInputByte();
    ctx->mBytesTransmitted += input_bytes;
    ctx->mCurrentByte += input_bytes;
    ctx->mChunksCompleted++;

    // DEBUG: Log buffer completion details
    FL_LOG_PARLIO("PARLIO ISR: Buffer " << completed_buffer_idx
           << " COMPLETED | dma_bytes=" << dma_bytes
           << " | input_bytes=" << input_bytes
           << " | total_tx=" << ctx->mBytesTransmitted);

    // ISR-based streaming: Check if next buffer is ready in the ring (use count to detect empty ring)
    volatile size_t count = ctx->mRingCount;

    // Ring empty - check if all data transmitted
    if (count == 0) {
        // Ring is empty - check if we've transmitted ALL the data
        if (ctx->mBytesTransmitted >= ctx->mTotalBytes) {
            // All data transmitted - mark transmission complete
            FL_LOG_PARLIO("PARLIO ISR: Transmission COMPLETE | transmitted="
                   << ctx->mBytesTransmitted << " | total=" << ctx->mTotalBytes);
            ctx->mStreamComplete = true;
            ctx->mTransmitting = false;
            return false; // No high-priority task woken
        }

        // Ring empty but more data pending - wait for CPU to populate more buffers
        FL_LOG_PARLIO("PARLIO ISR: Ring empty, waiting for CPU | transmitted="
               << ctx->mBytesTransmitted << "/" << ctx->mTotalBytes);
        return false; // No high-priority task woken
    }

    // Next buffer is ready - submit it to hardware
    size_t buffer_idx = read_idx;
    uint8_t *buffer_ptr = self->mState.mRingBuffers[buffer_idx].get();
    size_t buffer_size = self->mState.mRingBufferSizes[buffer_idx];

    // Invalid buffer - set error flag
    if (!buffer_ptr || buffer_size == 0) {
        ctx->mRingError = true;
        return false;
    }

    // DEBUG: Log ISR buffer submission (careful - this is ISR context!)
    // Note: FL_LOG_PARLIO uses printf which is safe in ISR if properly configured
    FL_LOG_PARLIO("PARLIO ISR: Submitting buffer " << buffer_idx
           << " | size=" << buffer_size
           << " | bits=" << (buffer_size * 8));

    // Submit buffer to hardware
    parlio_transmit_config_t tx_config = {};
    tx_config.idle_value = 0x0000; // Keep pins LOW between chunks

    esp_err_t err = parlio_tx_unit_transmit(tx_unit, buffer_ptr,
                                            buffer_size * 8, &tx_config);

    if (err == ESP_OK) {
        // Successfully submitted - advance read index (modulo-3) and decrement count
        ctx->mRingReadIdx = (ctx->mRingReadIdx + 1) % PARLIO_RING_BUFFER_COUNT;
        ctx->mRingCount = ctx->mRingCount - 1;
        FL_LOG_PARLIO("PARLIO ISR: Buffer " << buffer_idx << " submitted OK");
    } else {
        // Submission failed - set error flag for CPU to detect
        ctx->mRingError = true;
        FL_WARN("PARLIO ISR: Buffer " << buffer_idx << " submission failed: " << err);
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
    uint8_t* laneWaveforms = mState.mWaveformExpansionBuffer.get();
    constexpr size_t bytes_per_lane = sizeof(Wave8Byte); // 8 bytes per input byte

    size_t outputIdx = 0;
    size_t byteOffset = 0;

    // Two-stage architecture: Process one byte position at a time
    // Stage 1: Generate wave8bytes for ALL lanes → staging buffer
    // Stage 2: Transpose staging buffer → DMA output buffer

    // Use calculator for transpose block size
    ParlioBufferCalculator calc{mState.mDataWidth};
    size_t blockSize = calc.transposeBlockSize();

    while (byteOffset < byteCount) {
        // Check if enough space for this block
        if (outputIdx + blockSize > outputBufferCapacity) {
            // Buffer overflow - return error immediately
            outputBytesWritten = outputIdx;
            return false;
        }

        // ═══════════════════════════════════════════════════════════════
        // STAGE 1: Generate wave8bytes for ALL lanes into staging buffer
        // ═══════════════════════════════════════════════════════════════
        for (size_t lane = 0; lane < mState.mDataWidth; lane++) {
            uint8_t* laneWaveform = laneWaveforms + (lane * bytes_per_lane);

            if (lane < mState.mActualChannels) {
                // Real channel - expand using wave8
                const uint8_t* laneData = mState.mScratchPaddedBuffer.data() +
                                          (lane * mState.mLaneStride);
                uint8_t byte = laneData[startByte + byteOffset];

                // wave8() outputs Wave8Byte (8 bytes) in bit-packed format
                // Cast pointer to array reference for wave8 API
                uint8_t (*wave8Array)[8] = reinterpret_cast<uint8_t (*)[8]>(laneWaveform);
                fl::wave8(byte, mState.mWave8Lut, *wave8Array);
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
            mState.mDataWidth,    // Number of lanes (1-16)
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

    // Use count to determine if ring has space (distinguishes full vs empty)
    volatile size_t count = mState.mIsrContext->mRingCount;

    // Ring has space if count is less than 3
    return count < PARLIO_RING_BUFFER_COUNT;
}

bool ChannelEnginePARLIOImpl::generateRingBuffer() {
    // One-time ring buffer allocation and initialization
    // Called once during initializeIfNeeded(), NOT per transmission
    // Buffers remain allocated, only POPULATED on-demand during transmission

    // Clear any existing ring buffers
    mState.mRingBuffers.clear();
    mState.mRingBufferSizes.clear();

    // Allocate all ring buffers with DMA capability
    for (size_t i = 0; i < PARLIO_RING_BUFFER_COUNT; i++) {
        fl::unique_ptr<uint8_t[], HeapCapsDeleter> buffer(
            static_cast<uint8_t *>(heap_caps_malloc(
                mState.mRingBufferCapacity, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL)));

        if (!buffer) {
            FL_WARN("PARLIO: Failed to allocate ring buffer "
                    << i << "/" << PARLIO_RING_BUFFER_COUNT << " (requested "
                    << mState.mRingBufferCapacity << " bytes)");
            // Clean up already allocated ring buffers (automatic via unique_ptr destructors)
            mState.mRingBuffers.clear();
            mState.mRingBufferSizes.clear();
            return false;
        }

        // Zero-initialize buffer to prevent garbage data
        fl::memset(buffer.get(), 0x00, mState.mRingBufferCapacity);

        mState.mRingBuffers.push_back(fl::move(buffer));
        mState.mRingBufferSizes.push_back(0); // Will be set during population
    }

    return true;
}

bool ChannelEnginePARLIOImpl::populateNextDMABuffer() {
    // Incremental buffer population - called repeatedly to fill ring buffers
    // Returns true if more buffers need to be populated, false if all done

    if (!mState.mIsrContext) {
        return false;
    }

    // Check if more data to process
    if (mState.mNextByteOffset >= mState.mIsrContext->mTotalBytes) {
        return false; // No more source data
    }

    // Get next ring buffer index (0, 1, or 2)
    size_t ring_index = mState.mNextPopulateIdx;

    // Get ring buffer pointer
    uint8_t *outputBuffer = mState.mRingBuffers[ring_index].get();
    if (!outputBuffer) {
        FL_WARN("PARLIO: Ring buffer " << ring_index << " not allocated");
        mState.mErrorOccurred = true;
        return false;
    }

    // Calculate byte range for this buffer (divide total bytes into ~3 chunks)
    size_t bytes_remaining = mState.mIsrContext->mTotalBytes - mState.mNextByteOffset;
    size_t bytes_per_buffer = (mState.mIsrContext->mTotalBytes + PARLIO_RING_BUFFER_COUNT - 1) / PARLIO_RING_BUFFER_COUNT;

    // CAP bytes_per_buffer at ring buffer capacity to enable streaming for large strips
    // Without this cap, large LED strips (e.g., 3000 LEDs) would try to fit 3000 bytes into
    // a buffer sized for 100 LEDs, causing buffer overflow
    ParlioBufferCalculator calc{mState.mDataWidth};
    size_t max_input_bytes_per_buffer = mState.mRingBufferCapacity / calc.outputBytesPerInputByte();
    if (bytes_per_buffer > max_input_bytes_per_buffer) {
        bytes_per_buffer = max_input_bytes_per_buffer;
    }

    // LED boundary alignment: Round DOWN to nearest multiple of (3 bytes × lane count)
    // This prevents buffer splits from occurring mid-LED across all lanes, which causes bit shift corruption
    size_t bytes_per_led_all_lanes = 3 * mState.mDataWidth;  // 3 bytes (RGB) × lane count
    bytes_per_buffer = (bytes_per_buffer / bytes_per_led_all_lanes) * bytes_per_led_all_lanes;

    // Edge case: Ensure at least one LED across all lanes per buffer if data exists
    if (bytes_per_buffer < bytes_per_led_all_lanes && mState.mIsrContext->mTotalBytes >= bytes_per_led_all_lanes) {
        bytes_per_buffer = bytes_per_led_all_lanes;
    }

    // FIX: For the LAST buffer, take ALL remaining bytes (don't round down and lose data)
    // This prevents data loss when the remaining bytes don't evenly divide by LED boundary
    // Detect last buffer: when populated buffers would reach or exceed ring capacity (3 buffers)
    size_t byte_count;
    size_t buffers_already_populated = mState.mIsrContext->mRingCount;
    bool is_last_buffer = (buffers_already_populated >= PARLIO_RING_BUFFER_COUNT - 1) ||
                          (bytes_remaining <= bytes_per_buffer);
    if (is_last_buffer) {
        byte_count = bytes_remaining;  // Last buffer takes all remaining bytes
        // BUT cap at buffer capacity (streaming will handle rest)
        if (byte_count > max_input_bytes_per_buffer) {
            byte_count = max_input_bytes_per_buffer;
        }
    } else {
        byte_count = bytes_per_buffer;  // Earlier buffers use aligned size
    }

    // Zero output buffer to prevent garbage data from previous use
    fl::memset(outputBuffer, 0x00, mState.mRingBufferCapacity);

    // Generate waveform data using helper function
    size_t outputBytesWritten = 0;
    if (!populateDmaBuffer(outputBuffer, mState.mRingBufferCapacity,
                          mState.mNextByteOffset, byte_count, outputBytesWritten)) {
        FL_WARN("PARLIO: Ring buffer overflow at ring_index=" << ring_index);
        mState.mErrorOccurred = true;
        return false;
    }

    // Store actual size of this buffer
    mState.mRingBufferSizes[ring_index] = outputBytesWritten;

    // DEBUG: Log buffer population details
    FL_LOG_PARLIO("PARLIO: Populated buffer " << ring_index
           << " | input bytes " << mState.mNextByteOffset << "-" << (mState.mNextByteOffset + byte_count - 1)
           << " | byte_count=" << byte_count
           << " | DMA bytes=" << outputBytesWritten);

    // Update state for next buffer
    mState.mNextByteOffset += byte_count;
    mState.mNextPopulateIdx = (mState.mNextPopulateIdx + 1) % PARLIO_RING_BUFFER_COUNT;

    // Signal ISR that buffer is ready (advance write index and increment count)
    mState.mIsrContext->mRingWriteIdx = (mState.mIsrContext->mRingWriteIdx + 1) % PARLIO_RING_BUFFER_COUNT;
    mState.mIsrContext->mRingCount = mState.mIsrContext->mRingCount + 1;

    // Return true if more bytes remain to be processed
    return mState.mNextByteOffset < mState.mIsrContext->mTotalBytes;
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
    if (!mInitialized || mState.mTxUnit == nullptr || !mState.mIsrContext) {
        return EngineState::READY;
    }

    // Check for ISR-reported errors
    if (mState.mErrorOccurred) {
        FL_WARN("PARLIO: Error occurred during streaming transmission");
        mState.mIsrContext->mTransmitting = false;
        mState.mErrorOccurred = false;
        return EngineState::ERROR;
    }

    // Phase 4: Check if streaming is complete (volatile read, no barrier yet)
    // Read stream_complete first (volatile field ensures fresh read from
    // memory)
    if (mState.mIsrContext->mStreamComplete) {
        // Execute memory barrier to synchronize all ISR writes
        // This ensures all non-volatile ISR data (mIsrCount, mBytesTransmitted,
        // etc.) is visible
        PARLIO_MEMORY_BARRIER();

        // Clear completion flags BEFORE cleanup operations (volatile
        // assignment) This prevents race condition where new transmission could
        // start during cleanup Previous code cleared flags AFTER cleanup,
        // allowing state corruption
        mState.mIsrContext->mTransmitting = false;
        mState.mIsrContext->mStreamComplete = false;

        // Wait for final chunk to complete
        esp_err_t err = parlio_tx_unit_wait_all_done(mState.mTxUnit, 0);

        if (err == ESP_OK) {
            // FIX C5-1 PARTIAL REVERT: GPIO reset loop IS needed (Iteration 21
            // fix was correct) Analysis showed Patterns B-D fail without GPIO
            // stabilization between transmissions PARLIO idle_value alone is
            // insufficient to prevent GPIO glitches on subsequent starts
            // OPTIMIZATION: Reduced delay from 1ms → 100μs (10x faster, still
            // prevents glitches)

            // Disable PARLIO to reset peripheral state
            err = parlio_tx_unit_disable(mState.mTxUnit);
            if (err != ESP_OK) {
                FL_WARN("PARLIO: Failed to disable TX unit after transmission: "
                        << err);
            }

            // Reduced delay: 100μs instead of 1ms (10x faster, 2% overhead
            // instead of 20%)
            fl::delayMicros(100);


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
    if (!mState.mIsrContext->mTransmitting) {
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
    if (required_width != mState.mDataWidth) {
        FL_WARN("PARLIO: Channel count "
                << channel_count << " requires data_width=" << required_width
                << " but this instance is data_width=" << mState.mDataWidth);
        return;
    }

    // Store actual channel count (for dummy lane logic)
    mState.mActualChannels = channel_count;
    mState.mDummyLanes = mState.mDataWidth - channel_count;

    if (mState.mDummyLanes > 0) {
    }

    // Extract and validate GPIO pins from channel data
    mState.mPins.clear();
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

        mState.mPins.push_back(pin);
    }

    // Fill remaining pins with -1 for dummy lanes (unused)
    for (size_t i = channel_count; i < mState.mDataWidth; i++) {
        mState.mPins.push_back(-1);
    }

    // FIX C1-3: Extract timing configuration BEFORE initializeIfNeeded()
    // This ensures waveform generation has correct timing parameters from the
    // start Previously, timing was set AFTER init, causing first frame to use
    // uninitialized values (0,0,0)
    const ChipsetTimingConfig &timing = channelData[0]->getTiming();
    mState.mTimingT1Ns = timing.t1_ns;
    mState.mTimingT2Ns = timing.t2_ns;
    mState.mTimingT3Ns = timing.t3_ns;

    // Ensure PARLIO peripheral is initialized
    // Note: initializeIfNeeded() uses mState.timing_* to configure waveforms
    initializeIfNeeded();

    // If initialization failed, abort
    if (!mInitialized || mState.mTxUnit == nullptr || !mState.mIsrContext) {
        FL_WARN("PARLIO: Cannot transmit - initialization failed");
        return;
    }

    // Check if we're already transmitting
    if (mState.mIsrContext->mTransmitting) {
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
    mState.mScratchPaddedBuffer.resize(totalBufferSize);

    // FIX H3-4: Removed redundant scratch buffer zero-fill (1-2ms savings per
    // frame) Subsequent loop (lines 862-882) overwrites all bytes via memcpy +
    // right-padding memset Pattern D corruption (Iteration 22) was fixed by
    // right-padding logic, not this memset

    mState.mLaneStride = maxChannelSize;
    mState.mNumLanes = channelData.size();
    // Phase 5: Also set num_lanes in ISR context (ISR needs this for
    // mBytesTransmitted calculation)
    mState.mIsrContext->mNumLanes = channelData.size();

    // Write all channels to segmented scratch buffer with stride-based layout
    for (size_t i = 0; i < channelData.size(); i++) {
        size_t dataSize = channelData[i]->getSize();
        uint8_t *laneDst =
            mState.mScratchPaddedBuffer.data() + (i * maxChannelSize);
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

    // Ring buffer streaming architecture with CPU-based waveform generation
    // Initialize IsrContext state for ring buffer streaming
    mState.mIsrContext->mTotalBytes = totalBytes;
    mState.mIsrContext->mCurrentByte = 0;
    mState.mIsrContext->mStreamComplete = false;
    mState.mErrorOccurred = false;
    mState.mIsrContext->mTransmitting =
        false; // Will be set to true after first buffer submitted

    // Initialize ring buffer indices (0-2 only) and count
    mState.mIsrContext->mRingReadIdx = 0;
    mState.mIsrContext->mRingWriteIdx = 0;
    mState.mIsrContext->mRingCount = 0;
    mState.mIsrContext->mRingError = false;

    // Initialize counters
    mState.mIsrContext->mIsrCount = 0;
    mState.mIsrContext->mBytesTransmitted = 0;
    mState.mIsrContext->mChunksCompleted = 0;
    mState.mIsrContext->mTransmissionActive = true;
    mState.mIsrContext->mEndTimeUs = 0;

    // Initialize population state
    mState.mNextPopulateIdx = 0;
    mState.mNextByteOffset = 0;

    // Pre-populate ring buffers (fill all 3 buffers if possible)
    // This ensures ISR has buffers ready when transmission starts
    // Remaining data will be populated incrementally during poll()
    // Phase 3 - Iteration 8: Simplify buffer counting by reading mRingCount directly
    // The loop increments mRingCount inside populateNextDMABuffer(), so we can just
    // read the final value after the loop completes.
    while (has_ring_space() && populateNextDMABuffer()) {
        // populateNextDMABuffer() increments mRingCount and returns true if more data
    }

    // Get actual number of buffers populated from mRingCount
    size_t buffers_populated = mState.mIsrContext->mRingCount;

    // Verify at least one buffer was populated
    if (buffers_populated == 0) {
        FL_WARN("PARLIO: No buffers populated - cannot start transmission");
        mState.mErrorOccurred = true;
        return;
    }

    // Enable PARLIO TX unit for this transmission
    // ESP-IDF PARLIO state machine:
    //   - init state → enable() → enabled state (ready to transmit)
    //   - enabled state → disable() → init state (after transmission done in poll())
    // After initializeIfNeeded(): unit is in "init" state
    // After poll() completes transmission: unit is in "init" state (disabled)
    // This enable() transitions from "init" to "enabled" for this transmission
    esp_err_t err = parlio_tx_unit_enable(mState.mTxUnit);
    if (err != ESP_OK) {
        FL_WARN("PARLIO: Failed to enable TX unit for transmission: " << err);
        mState.mErrorOccurred = true;
        return;
    }

    // Phase 2: ISR-based streaming - Queue ONLY the first buffer, ISR queues the rest
    // The first buffer is already in the ring (mRingCount >= 1)
    // ISR will queue subsequent buffers as each buffer completes
    FL_LOG_PARLIO("PARLIO: Starting ISR-based streaming | first_buffer_size="
           << mState.mRingBufferSizes[0] << " | buffers_ready=" << buffers_populated);

    parlio_transmit_config_t tx_config = {};
    tx_config.idle_value = 0x0000; // Start with pins LOW

    // Submit ONLY the first buffer to start transmission
    // ISR will handle queuing subsequent buffers
    size_t first_buffer_size = mState.mRingBufferSizes[0];

    FL_LOG_PARLIO("PARLIO: Queuing first buffer (idx=0) | size=" << first_buffer_size
           << " | bits=" << (first_buffer_size * 8));

    err = parlio_tx_unit_transmit(
        mState.mTxUnit, mState.mRingBuffers[0].get(),
        first_buffer_size * 8, &tx_config);

    if (err != ESP_OK) {
        FL_WARN("PARLIO: Failed to queue first buffer: " << err);
        mState.mErrorOccurred = true;
        return;
    }

    FL_LOG_PARLIO("PARLIO: First buffer queued OK - ISR will handle subsequent buffers");

    // Advance read index to consume the first buffer
    // ISR will advance it further as it queues more buffers
    mState.mIsrContext->mRingReadIdx = 1;
    mState.mIsrContext->mRingCount = buffers_populated - 1;

    // Mark transmission started
    mState.mIsrContext->mTransmitting = true;

    // Phase 2: Block here and continue populating buffers as ISR consumes them
    // This ensures all data gets transmitted before returning
    // The ISR queues buffers, and we refill the ring from CPU side
    FL_LOG_PARLIO("PARLIO: Entering blocking loop | mNextByteOffset=" << mState.mNextByteOffset
           << " | mTotalBytes=" << mState.mIsrContext->mTotalBytes);

    while (mState.mNextByteOffset < mState.mIsrContext->mTotalBytes) {
        FL_LOG_PARLIO("PARLIO: Loop iteration | offset=" << mState.mNextByteOffset
               << "/" << mState.mIsrContext->mTotalBytes
               << " | has_space=" << has_ring_space()
               << " | transmitting=" << mState.mIsrContext->mTransmitting
               << " | ring_count=" << mState.mIsrContext->mRingCount);

        // Wait for ring to have space (ISR consuming buffers)
        while (!has_ring_space() && mState.mIsrContext->mTransmitting) {
            fl::delayMicroseconds(10); // Yield to ISR
        }

        // Check if transmission completed or errored
        if (!mState.mIsrContext->mTransmitting || mState.mErrorOccurred) {
            FL_LOG_PARLIO("PARLIO: Loop exit - transmission stopped | transmitting="
                   << mState.mIsrContext->mTransmitting << " | error=" << mState.mErrorOccurred);
            break;
        }

        // Populate next buffer if more data remains
        if (mState.mNextByteOffset < mState.mIsrContext->mTotalBytes) {
            FL_LOG_PARLIO("PARLIO: About to populate next buffer | offset=" << mState.mNextByteOffset);
            if (!populateNextDMABuffer()) {
                FL_WARN("PARLIO: Failed to populate buffer during transmission");
                mState.mErrorOccurred = true;
                break;
            }
            FL_LOG_PARLIO("PARLIO: Buffer populated | new_offset=" << mState.mNextByteOffset);
        }
    }

    FL_LOG_PARLIO("PARLIO: Exited blocking loop | final_offset=" << mState.mNextByteOffset
           << " | expected=" << mState.mIsrContext->mTotalBytes
           << " | error=" << mState.mErrorOccurred);

    FL_LOG_PARLIO("PARLIO: All buffers queued - transmission in progress");
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
    if (mState.mDataWidth > SOC_PARLIO_TX_UNIT_MAX_DATA_WIDTH) {
        FL_WARN(
            "PARLIO: Requested data width "
            << mState.mDataWidth << " bits exceeds hardware maximum "
            << SOC_PARLIO_TX_UNIT_MAX_DATA_WIDTH << " bits on this chip. "
            << "This chip does not support " << mState.mDataWidth
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
    if (mState.mDataWidth > 16) {
        FL_WARN("PARLIO: Requested data width "
                << mState.mDataWidth
                << " bits exceeds assumed maximum (16 bits). "
                << "Hardware capability unknown - update ESP-IDF for proper "
                   "detection. "
                << "Proceeding with caution...");
        // Don't fail - older IDF may still work, just warn
    }
#endif

    // Step 1: Build wave8 expansion LUT from timing configuration
    // Timing parameters are extracted from the first channel in
    // beginTransmission() and stored in mState.mTimingT1Ns,
    // mState.mTimingT2Ns, mState.mTimingT3Ns
    //
    // For WS2812B-V5 (typical values):
    // - T1: 300ns (T0H - initial HIGH time)
    // - T2: 200ns (T1H-T0H - additional HIGH time for bit 1)
    // - T3: 500ns (T0L - LOW tail duration)
    //
    // wave8 normalizes timing to 8 pulses per bit (fixed 8:1 expansion)
    ChipsetTiming timing;
    timing.T1 = mState.mTimingT1Ns;
    timing.T2 = mState.mTimingT2Ns;
    timing.T3 = mState.mTimingT3Ns;
    timing.RESET = 0; // Not used for waveform generation
    timing.name = "PARLIO";

    mState.mWave8Lut = buildWave8ExpansionLUT(timing);
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
    if (mState.mDataWidth <= 8) {
        // After transpose: 8 pulses × 8 bits / (8/data_width) ticks per byte
        // For data_width=1: 8*8 / 8 = 8 bytes per input byte
        bytes_per_input_byte = 8 * pulses_per_bit / (8 / mState.mDataWidth);
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
    mState.mBytesPerChunk = target_buffer_size / bytes_per_input_byte;

    // Clamp to reasonable range (in input bytes)
    if (mState.mBytesPerChunk < 10)
        mState.mBytesPerChunk = 10;
    if (mState.mBytesPerChunk > 5000)
        mState.mBytesPerChunk = 5000;

    // Step 3: Validate GPIO pins were provided by beginTransmission()
    // Pins are extracted from ChannelData and validated before initialization
    if (mState.mPins.size() != mState.mDataWidth) {
        FL_WARN("PARLIO: Pin configuration error - expected "
                << mState.mDataWidth << " pins, got " << mState.mPins.size());
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
    config.data_width = mState.mDataWidth; // Runtime parameter
    // Phase 3 - Iteration 6: Set trans_queue_depth to 3 (matches ring buffer count)
    // PARLIO has 3 internal queues (READY, PROGRESS, COMPLETE) per README.md line 101
    // Root cause: With depth=2, when ISR fires:
    //   - Completed transaction still in COMPLETE queue (slot 1)
    //   - New transmission in PROGRESS queue (slot 2)
    //   - No room for ISR to queue 3rd buffer → transmission stops at buffer 1
    // Fix: depth=3 allows all 3 ring buffers to be queued simultaneously
    // Testing: depth=2 transmits only ~968 bytes (1 buffer); depth=3 should transmit full 3000 bytes
    config.trans_queue_depth = 3;
    // Phase 3 - Iteration 19: ESP-IDF PARLIO hardware limit is 65535 BITS per
    // transmission This is a hard hardware limit that cannot be exceeded For
    // transmissions larger than this, we must chunk into multiple calls Set
    // max_transfer_size to hardware maximum (must be < 65535, so use 65534)
    config.max_transfer_size =
        65534; // Hardware maximum (65535 bits - 1 for safety)
    config.bit_pack_order =
        PARLIO_BIT_PACK_ORDER_LSB; // Lane 0 = bit 0, transmit bit 0 first
    config.sample_edge = PARLIO_SAMPLE_EDGE_POS; // Positive edge sampling (required for WS2812B)

    // Assign GPIO pins for data_width lanes
    for (size_t i = 0; i < mState.mDataWidth; i++) {
        config.data_gpio_nums[i] = static_cast<gpio_num_t>(mState.mPins[i]);
    }
    // Mark unused pins as invalid
    for (size_t i = mState.mDataWidth; i < 16; i++) {
        config.data_gpio_nums[i] = static_cast<gpio_num_t>(-1);
    }

    config.clk_out_gpio_num = static_cast<gpio_num_t>(-1);
    config.valid_gpio_num = static_cast<gpio_num_t>(-1);

    // Step 5: Create TX unit
    esp_err_t err = parlio_new_tx_unit(&config, &mState.mTxUnit);
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

    err = parlio_tx_unit_register_event_callbacks(mState.mTxUnit, &callbacks,
                                                  this);
    if (err != ESP_OK) {
        FL_WARN("PARLIO: Failed to register callbacks: " << err);
        parlio_del_tx_unit(mState.mTxUnit);
        mState.mTxUnit = nullptr;
        mInitialized = false;
        return;
    }

    // Step 7: TX unit created and callbacks registered
    // NOTE: Unit remains in "init" state (NOT enabled yet)
    // Enable will be called by beginTransmission() before each transmission
    // This follows the enable-when-needed pattern recommended for peripheral management

    // Step 8: Allocate double buffers for ping-pong streaming
    // CRITICAL REQUIREMENTS:
    // 1. MUST be DMA-capable memory (MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL)
    //    Required for PARLIO/GDMA hardware to access these buffers
    // 2. MUST be heap-allocated (NOT stack) - accessed by DMA hardware
    // asynchronously
    // 3. Buffer size depends on data_width and actual pulses_per_bit
    // (calculated at runtime)

    // Calculate ring buffer capacity based on LED frame boundaries and wave8 expansion
    // Default sizing: 3000 LEDs per channel (1000 LEDs per buffer × 3 ring buffers)
    // This ensures proper DMA chunking for large LED strips with multiple boundaries
    ParlioBufferCalculator calc{mState.mDataWidth};

    mState.mRingBufferCapacity = calc.calculateRingBufferCapacity(MAX_LEDS_PER_CHANNEL, PARLIO_RING_BUFFER_COUNT);

    // Step 8: Allocate ring buffers upfront (all memory allocated during init)
    // Buffers will be populated on-demand during transmission
    if (!generateRingBuffer()) {
        FL_WARN("PARLIO: Failed to allocate ring buffers during initialization");

        // Clean up TX unit
        esp_err_t disable_err = parlio_tx_unit_disable(mState.mTxUnit);
        if (disable_err != ESP_OK) {
            FL_WARN("PARLIO: Failed to disable TX unit during ring buffer cleanup: "
                    << disable_err);
        }
        esp_err_t del_err = parlio_del_tx_unit(mState.mTxUnit);
        if (del_err != ESP_OK) {
            FL_WARN("PARLIO: Failed to delete TX unit during ring buffer cleanup: "
                    << del_err);
        }
        mState.mTxUnit = nullptr;
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
    // 3. Ring buffers (allocated in generateRingBuffer) MUST be DMA-capable for
    // PARLIO/GDMA hardware
    // 4. This buffer is managed by mState and freed in the destructor
    const size_t bytes_per_lane = sizeof(Wave8Byte); // 8 bytes per input byte
    const size_t waveform_buffer_size = mState.mDataWidth * bytes_per_lane;

    mState.mWaveformExpansionBuffer.reset(static_cast<uint8_t *>(
        heap_caps_malloc(waveform_buffer_size, MALLOC_CAP_INTERNAL)));

    if (!mState.mWaveformExpansionBuffer) {
        FL_WARN("PARLIO: Failed to allocate waveform expansion buffer ("
                << waveform_buffer_size << " bytes)");

        // Clean up TX unit
        esp_err_t disable_err = parlio_tx_unit_disable(mState.mTxUnit);
        if (disable_err != ESP_OK) {
            FL_WARN("PARLIO: Failed to disable TX unit during waveform buffer "
                    "cleanup: "
                    << disable_err);
        }
        esp_err_t del_err = parlio_del_tx_unit(mState.mTxUnit);
        if (del_err != ESP_OK) {
            FL_WARN("PARLIO: Failed to delete TX unit during waveform buffer "
                    "cleanup: "
                    << del_err);
        }
        mState.mTxUnit = nullptr;
        mInitialized = false;
        return;
    }

    mState.mWaveformExpansionBufferSize = waveform_buffer_size;
    // Phase 4: Initialize IsrContext state (if allocated)
    if (mState.mIsrContext) {
        mState.mIsrContext->mTransmitting = false;
        mState.mIsrContext->mStreamComplete = false;
        mState.mIsrContext->mCurrentByte = 0;
        mState.mIsrContext->mTotalBytes = 0;
    }
    mState.mErrorOccurred = false;

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

    // Read ISR counters
    metrics.mIsrCount = ctx->mIsrCount;
    metrics.mChunksQueued = 0;  // No longer tracked
    metrics.mChunksCompleted = ctx->mChunksCompleted;
    metrics.mBytesTotal = ctx->mTotalBytes;
    metrics.mBytesTransmitted = ctx->mBytesTransmitted;

    metrics.mTransmissionActive = ctx->mTransmitting;
    metrics.mStartTimeUs = 0; // Not currently tracked
    metrics.mEndTimeUs = ctx->mEndTimeUs;
    metrics.mErrorCode = 0; // ESP_OK

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
