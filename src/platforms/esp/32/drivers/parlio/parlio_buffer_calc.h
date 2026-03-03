/// @file parlio_buffer_calc.h
/// @brief Buffer size calculations for PARLIO DMA buffers
///
/// Provides unified calculations for PARLIO buffer sizing, LED boundary alignment,
/// and reset time padding. All calculations are pure math with no hardware dependencies.

#pragma once

// IWYU pragma: private

#include "platforms/esp/is_esp.h"

#include "fl/compiler_control.h"
#include "fl/log.h"

namespace fl {
namespace detail {

// Total DMA ring buffer memory cap (all 3 ring buffers combined)
// Prevents OOM on constrained platforms while allowing streaming for large LED counts
// Override via build flags: -DFASTLED_PARLIO_MAX_RING_BUFFER_TOTAL_BYTES=<bytes>
#ifndef FASTLED_PARLIO_MAX_RING_BUFFER_TOTAL_BYTES
  #if defined(FL_IS_ESP_32C6) || defined(FL_IS_ESP_32S3)
    // ESP32-C6/S3: 256 KB total (fits in ~512 KB SRAM, leaves room for other allocations)
    #define FASTLED_PARLIO_MAX_RING_BUFFER_TOTAL_BYTES (256UL * 1024UL)
  #elif defined(FL_IS_ESP_32P4)
    // ESP32-P4: 512 KB total (better performance, larger SRAM available)
    #define FASTLED_PARLIO_MAX_RING_BUFFER_TOTAL_BYTES (512UL * 1024UL)
  #else
    // Conservative default for unknown platforms
    #define FASTLED_PARLIO_MAX_RING_BUFFER_TOTAL_BYTES (256UL * 1024UL)
  #endif
#endif

// Minimum cap validation (supports at least 1 LED × 16 lanes)
static_assert(FASTLED_PARLIO_MAX_RING_BUFFER_TOTAL_BYTES >= 12UL * 1024UL,
              "FASTLED_PARLIO_MAX_RING_BUFFER_TOTAL_BYTES too small (minimum 12 KB)");

// PSRAM ring buffer memory cap (used when PSRAM is available at runtime)
// Much larger than internal SRAM cap since PSRAM provides ~8MB on ESP32-P4
// Override via build flags: -DFASTLED_PARLIO_MAX_RING_BUFFER_TOTAL_BYTES_PSRAM=<bytes>
#ifndef FASTLED_PARLIO_MAX_RING_BUFFER_TOTAL_BYTES_PSRAM
  #if defined(FL_IS_ESP_32P4)
    // ESP32-P4: 2 MB (has ~8MB PSRAM, generous allocation for 5+ channels)
    #define FASTLED_PARLIO_MAX_RING_BUFFER_TOTAL_BYTES_PSRAM (2UL * 1024UL * 1024UL)
  #else
    // Other platforms: 1 MB (conservative default)
    #define FASTLED_PARLIO_MAX_RING_BUFFER_TOTAL_BYTES_PSRAM (1UL * 1024UL * 1024UL)
  #endif
#endif

//=============================================================================
// Buffer Size Calculator - Unified DMA Buffer Size Calculations
//=============================================================================

/// @brief Unified calculator for PARLIO buffer sizes
///
/// Consolidates all buffer size calculations into a single, tested utility.
/// Wave8 expands each input byte to 64 pulses (8 bits × 8 pulses per bit).
/// Transposition packs pulses into bytes based on data_width.
struct ParlioBufferCalculator {
    size_t mDataWidth;

    /// @brief Calculate output bytes per input byte after wave8 + transpose
    /// @return Output bytes per input byte (8 for width≤8, 128 for width=16)
    size_t outputBytesPerInputByte() const {
        if (mDataWidth <= 8) {
            // Bit-packed: 64 pulses packed into (8 / mDataWidth) ticks per byte
            // For data_width=1: 64 pulses / 8 ticks = 8 bytes
            // For data_width=2: 64 pulses / 4 ticks = 16 bytes
            // For data_width=4: 64 pulses / 2 ticks = 32 bytes
            // For data_width=8: 64 pulses / 1 tick = 64 bytes
            size_t ticksPerByte = 8 / mDataWidth;
            return (64 + ticksPerByte - 1) / ticksPerByte;
        } else if (mDataWidth == 16) {
            // 16-bit mode: 64 pulses × 2 bytes per pulse = 128 bytes
            return 128;
        }
        return 8; // Fallback
    }

    /// @brief Calculate boundary padding bytes (front/back padding for signal stability)
    /// @return Bytes needed for boundary padding (1 Wave8Byte per lane = 8 bytes)
    ///
    /// Phase 1: Adds front and back padding to eliminate boundary bit-flips.
    /// - Front padding: 1 Wave8Byte (8 bytes) of zeros before LED data
    /// - Back padding: 1 Wave8Byte (8 bytes) of zeros after LED data
    /// - Total: 16 bytes per lane (8 front + 8 back)
    ///
    /// For multi-lane (mDataWidth > 1), each lane gets its own padding.
    size_t boundaryPaddingBytes() const {
        // ITERATION 18: Zero front padding, keep back padding
        // Front padding caused phase shift (75% improvement when removed)
        // Back padding is REQUIRED for stable transmission (removing it makes things worse)
        constexpr size_t BYTES_PER_WAVE8 = 8;
        constexpr size_t FRONT_PAD_PER_LANE = 0;  // NO front padding (causes phase shift)
        constexpr size_t BACK_PAD_PER_LANE = BYTES_PER_WAVE8;   // 1 Wave8Byte (REQUIRED)

        // After transposition, padding is interleaved with lanes
        // For 1 lane: front=0, back=8 → total=8
        // For N lanes: Each lane contributes to transposed output
        // After transpose: 0 bytes front + 8 bytes back per lane
        return (FRONT_PAD_PER_LANE + BACK_PAD_PER_LANE) * mDataWidth;
    }

    /// @brief Calculate DMA buffer size for given input bytes (includes boundary + reset padding)
    /// @param inputBytes Number of input bytes to transmit
    /// @param reset_us Reset time in microseconds (default: 0)
    /// @return Total DMA buffer size in bytes (front_pad + pixel data + back_pad + reset_pad)
    size_t dmaBufferSize(size_t inputBytes, u32 reset_us = 0) const {
        size_t front_back_padding = boundaryPaddingBytes();
        size_t pixel_bytes = inputBytes * outputBytesPerInputByte();
        size_t reset_padding = resetPaddingBytes(reset_us);
        return front_back_padding + pixel_bytes + reset_padding;
    }

    /// @brief Calculate transpose output block size for populateDmaBuffer
    /// @return Block size in bytes for transpose operation
    size_t FL_IRAM transposeBlockSize() const {
        if (mDataWidth <= 8) {
            size_t ticksPerByte = 8 / mDataWidth;
            size_t pulsesPerByte = 64;
            return (pulsesPerByte + ticksPerByte - 1) / ticksPerByte;
        } else if (mDataWidth == 16) {
            return 128; // 64 pulses × 2 bytes per pulse
        }
        return 8; // Fallback
    }

    /// @brief Calculate additional bytes needed for reset time padding
    /// @param reset_us Reset time in microseconds
    /// @return Bytes to append for reset padding (all-zero Wave8Bytes)
    ///
    /// Calculation:
    /// - Each Wave8Byte = 64 pulses × 125ns (8MHz clock) = 8µs
    /// - Reset padding bytes = ceil(reset_us / 8µs) × 8 bytes
    /// - Example: 280µs reset ÷ 8µs = 35 Wave8Bytes = 280 bytes
    size_t resetPaddingBytes(u32 reset_us) const {
        if (reset_us == 0) {
            return 0;
        }

        // Each Wave8Byte covers 8µs (64 ticks at 8MHz)
        constexpr size_t US_PER_WAVE8BYTE = 8;

        // Calculate number of Wave8Bytes needed (round up)
        size_t num_wave8bytes = (reset_us + US_PER_WAVE8BYTE - 1) / US_PER_WAVE8BYTE;

        // Convert to byte count (8 bytes per Wave8Byte)
        return num_wave8bytes * 8;
    }

    /// @brief Calculate optimal ring buffer capacity based on LED frame boundaries
    /// @param maxLedsPerChannel Maximum LEDs per channel (strip size)
    /// @param reset_us Reset time in microseconds (for padding calculation)
    /// @param numRingBuffers Number of ring buffers (default: 3)
    /// @return DMA buffer capacity in bytes, aligned to LED boundaries
    ///
    /// Algorithm:
    /// 1. Calculate LEDs per buffer: maxLedsPerChannel / numRingBuffers
    /// 2. Convert to input bytes: LEDs × 3 bytes/LED × mDataWidth (multi-lane)
    /// 3. Apply wave8 expansion (8:1 ratio): input_bytes × outputBytesPerInputByte()
    /// 4. Add reset padding bytes (only to last buffer in stream)
    /// 5. Add safety margin for boundary checks
    /// 6. Result is DMA buffer capacity per ring buffer
    ///
    /// Example (3000 LEDs, 1 lane, 3 ring buffers, 280µs reset):
    /// - LEDs per buffer: 3000 / 3 = 1000 LEDs
    /// - Input bytes per buffer: 1000 × 3 × 1 = 3000 bytes
    /// - DMA bytes per buffer: 3000 × 8 = 24000 bytes
    /// - Reset padding: 280 bytes (35 Wave8Bytes × 8 bytes)
    /// - With safety margin: 24000 + 280 + 128 = 24408 bytes
    size_t calculateRingBufferCapacity(size_t maxLedsPerChannel, u32 reset_us, size_t numRingBuffers = 3,
                                       size_t totalCapBytes = FASTLED_PARLIO_MAX_RING_BUFFER_TOTAL_BYTES) const {
        size_t safetyMargin = 128;
        size_t perBufferCap = totalCapBytes / numRingBuffers;

        // CRITICAL: Try to fit the ENTIRE frame in a single DMA buffer.
        // The PARLIO peripheral has a ~20µs gap between DMA buffer transitions
        // (hardware limitation). This gap corrupts WS2812 signal timing and
        // causes bit-shift errors in all LEDs after the transition point.
        // By fitting all data in one buffer, we avoid transitions entirely.
        size_t totalInputBytes = maxLedsPerChannel * 3 * mDataWidth;
        size_t fullFrameDmaSize = dmaBufferSize(totalInputBytes, reset_us);

        if (fullFrameDmaSize + safetyMargin <= perBufferCap) {
            // Full frame fits in one ring buffer - no DMA transitions needed
            return fullFrameDmaSize + safetyMargin;
        }

        // Full frame doesn't fit - fall back to streaming with multiple ring buffers.
        // This path has the ~20µs DMA gap between buffers (unavoidable hardware limitation).
        size_t ledsPerBuffer = (maxLedsPerChannel + numRingBuffers - 1) / numRingBuffers;
        size_t inputBytesPerBuffer = ledsPerBuffer * 3 * mDataWidth;
        size_t dmaBufferCapacity = dmaBufferSize(inputBytesPerBuffer, reset_us);

        if (dmaBufferCapacity > perBufferCap) {
            size_t uncappedCapacity = dmaBufferCapacity;
            dmaBufferCapacity = perBufferCap;

            FL_LOG_PARLIO("PARLIO: Ring buffer capped at " << dmaBufferCapacity
                   << " bytes/buffer (uncapped: " << uncappedCapacity
                   << ", total cap: " << totalCapBytes << " bytes)");
        }

        dmaBufferCapacity += safetyMargin;

        return dmaBufferCapacity;
    }
};

} // namespace detail
} // namespace fl
