/// @file waveform_generator.h
/// @brief Generic waveform generator for clockless LED protocols using DMA/SPI
///
/// This module provides platform-agnostic waveform generation and transposition
/// for driving clockless LED protocols (WS2812, etc.) via DMA/SPI hardware.
///
/// # Design Overview
///
/// The waveform generator solves the problem of converting LED timing requirements
/// (T1, T2, T3) into hardware-compatible waveform patterns that can be sent via
/// DMA/SPI to produce the correct timing on GPIO pins.
///
/// ## Algorithm
///
/// 1. **GCD-based Resolution Calculation**
///    - Use GCD(T1, T2, T3) to find the optimal wave resolution
///    - Each bit is encoded as N pulses (where N = (T1+T2+T3)/GCD)
///    - Target: hit T1, T2, T3 timings within 75ns (never early, on-time or +75ns)
///    - Waveforms must be byte-aligned for DMA efficiency
///
/// 2. **Bit-to-Waveform Mapping**
///    - Bit 0: HIGH for T1, then LOW for T2+T3
///    - Bit 1: HIGH for T1+T2, then LOW for T3
///    - Each bit maps to a byte buffer pattern (N pulses)
///
/// 3. **Multi-Lane Transposition**
///    - Input: N lanes, each with a flat array of M segments
///    - For each segment:
///      - Grab one byte from each lane
///      - Convert each byte to waveform pattern (8 bits → 8 wave patterns)
///      - Transpose waveforms across lanes into DMA buffer
///    - Output: DMA buffer ready for hardware transmission
///
/// ## ISR Integration
///
/// The ISR callback operates in segments to avoid blocking:
/// - Assume 8 equal segments per frame (configurable)
/// - On each ISR trigger:
///   - Process next segment for all lanes
///   - Generate waveforms and transpose into DMA buffer
///   - Signal hardware to start transmission
///
/// @note This is designed to be platform-independent; platform-specific
///       code (IRAM attributes, DMA setup) should wrap these functions

#pragma once

#include "ftl/stdint.h"
#include "ftl/span.h"
#include "ftl/array.h"
#include "fl/compiler_control.h"

namespace fl {

// ============================================================================
// Timing Configuration
// ============================================================================

/// @brief LED protocol timing configuration (nanoseconds)
struct TimingConfig {
    uint32_t T1;        ///< High time for bit 0 (ns)
    uint32_t T2;        ///< Additional high time for bit 1 (ns)
    uint32_t T3;        ///< Low tail duration (ns)

    /// @brief Calculate total bit period
    FL_CONSTEXPR14 uint32_t getBitPeriod() const {
        return T1 + T2 + T3;
    }
};

// ============================================================================
// Waveform Configuration
// ============================================================================

/// @brief Result of waveform calculation
struct WaveformConfig {
    uint8_t pulsesPerBit;           ///< Number of pulses to encode one bit
    uint8_t pulsesForZero;          ///< Number of HIGH pulses for bit 0
    uint8_t pulsesForOne;           ///< Number of HIGH pulses for bit 1
    uint32_t resolution_ns;         ///< Time resolution per pulse (ns)
    bool valid;                     ///< True if configuration is valid

    /// @brief Check if configuration is valid
    FL_CONSTEXPR14 bool isValid() const {
        return valid && pulsesPerBit > 0 && pulsesPerBit <= 32;
    }
};

// ============================================================================
// Waveform Generator Functions
// ============================================================================

/// @brief Generate waveform pattern for bit 0
///
/// Creates a byte buffer representing the waveform for a logical 0 bit.
/// Each byte contains pulses where 0xFF = HIGH, 0x00 = LOW.
///
/// @param hz Clock frequency in Hz (e.g., 20000000 for 20MHz)
/// @param t1_ns T1 timing in nanoseconds (HIGH time for bit 0)
/// @param t2_ns T2 timing in nanoseconds (additional HIGH time for bit 1)
/// @param t3_ns T3 timing in nanoseconds (LOW tail duration)
/// @param b0_waveform Output buffer for bit 0 waveform pattern
/// @return Number of bytes written to b0_waveform
size_t generateBit0Waveform(
    uint32_t hz,
    uint32_t t1_ns,
    uint32_t t2_ns,
    uint32_t t3_ns,
    fl::span<uint8_t> b0_waveform
);

/// @brief Generate waveform pattern for bit 1
///
/// Creates a byte buffer representing the waveform for a logical 1 bit.
/// Each byte contains pulses where 0xFF = HIGH, 0x00 = LOW.
///
/// @param hz Clock frequency in Hz (e.g., 20000000 for 20MHz)
/// @param t1_ns T1 timing in nanoseconds (HIGH time for bit 0)
/// @param t2_ns T2 timing in nanoseconds (additional HIGH time for bit 1)
/// @param t3_ns T3 timing in nanoseconds (LOW tail duration)
/// @param b1_waveform Output buffer for bit 1 waveform pattern
/// @return Number of bytes written to b1_waveform
size_t generateBit1Waveform(
    uint32_t hz,
    uint32_t t1_ns,
    uint32_t t2_ns,
    uint32_t t3_ns,
    fl::span<uint8_t> b1_waveform
);

/// @brief Generate both bit 0 and bit 1 waveform patterns
///
/// Convenience function that generates both waveforms in one call.
///
/// @param hz Clock frequency in Hz (e.g., 20000000 for 20MHz)
/// @param t1_ns T1 timing in nanoseconds (HIGH time for bit 0)
/// @param t2_ns T2 timing in nanoseconds (additional HIGH time for bit 1)
/// @param t3_ns T3 timing in nanoseconds (LOW tail duration)
/// @param b0_waveform Output buffer for bit 0 waveform pattern
/// @param b0_size Output: number of bytes written to b0_waveform
/// @param b1_waveform Output buffer for bit 1 waveform pattern
/// @param b1_size Output: number of bytes written to b1_waveform
/// @return true if both waveforms generated successfully, false on error
bool generateWaveforms(
    uint32_t hz,
    uint32_t t1_ns,
    uint32_t t2_ns,
    uint32_t t3_ns,
    fl::span<uint8_t> b0_waveform,
    size_t& b0_size,
    fl::span<uint8_t> b1_waveform,
    size_t& b1_size
);

/// @brief Convert byte to waveform patterns (8 bits → 8 waveforms)
///
/// Takes a single byte and generates 8 waveform patterns (one per bit).
/// Each bit is expanded using the provided precomputed waveforms.
///
/// @param dataByte Input byte (8 bits of LED data)
/// @param b0_waveform Precomputed waveform for bit 0
/// @param b1_waveform Precomputed waveform for bit 1
/// @param output Output buffer (size must be >= 8 * pulsesPerBit)
/// @return Number of bytes written
///
/// @note Both b0_waveform and b1_waveform must be the same size (pulsesPerBit)
size_t expandByteToWaveforms(
    uint8_t dataByte,
    fl::span<const uint8_t> b0_waveform,
    fl::span<const uint8_t> b1_waveform,
    fl::span<uint8_t> output
);

// ============================================================================
// Multi-Lane Transposer Functions
// ============================================================================

/// @brief Maximum number of lanes supported
constexpr uint8_t MAX_LANES = 8;

/// @brief Transpose waveforms from multiple lanes into DMA buffer
///
/// Takes waveform data from up to 8 lanes and interleaves them into a
/// DMA buffer. This is optimized for the ISR context where one segment
/// of data is processed at a time.
///
/// @param laneData Array of lane data pointers (null for unused lanes)
/// @param numLanes Number of active lanes (1-8)
/// @param segmentLength Number of waveform bytes per lane in this segment
/// @param output DMA buffer output (size must be >= segmentLength * numLanes)
/// @return Number of bytes written to output
///
/// @note Input format: Each lane has segmentLength bytes of waveform data
/// @note Output format: Interleaved by pulse - [L0_pulse0, L1_pulse0, ..., L0_pulse1, L1_pulse1, ...]
size_t transposeLanes(
    const fl::span<const uint8_t>* laneData,
    uint8_t numLanes,
    size_t segmentLength,
    fl::span<uint8_t> output
);

/// @brief Transpose 8 lanes using optimized 8x8 bit matrix algorithm
///
/// This is a specialized version for exactly 8 lanes, using the optimized
/// 8x8 bit matrix transpose algorithm from Hacker's Delight.
///
/// @param laneData Array of 8 lane data spans
/// @param byteOffset Starting byte offset in each lane
/// @param output Output buffer (must have space for 8 bytes)
void transpose8Lanes(
    const fl::span<const uint8_t> laneData[8],
    size_t byteOffset,
    fl::span<uint8_t> output
);

// ============================================================================
// ISR Callback Interface
// ============================================================================

/// @brief ISR state for segmented waveform generation
///
/// This structure tracks the state needed for an ISR to incrementally
/// generate and transmit waveform data in segments.
struct ISRState {
    // Configuration
    uint8_t numLanes;                                    ///< Number of active lanes
    size_t bytesPerLane;                                 ///< Total bytes per lane
    size_t segmentSize;                                  ///< Bytes per segment per lane
    size_t pulsesPerBit;                                 ///< Number of pulses per bit

    // Precomputed waveforms
    fl::array<uint8_t, 64> zeroBitWave;                 ///< Waveform for bit 0
    fl::array<uint8_t, 64> oneBitWave;                  ///< Waveform for bit 1

    // Source data (LED pixel data)
    fl::span<const uint8_t> laneDataSources[8];         ///< Source data for each lane

    // Working buffers (allocated by platform code)
    fl::span<uint8_t> laneWaveformBuffers[8];           ///< Per-lane waveform buffers
    fl::span<uint8_t> dmaBuffer;                        ///< Final transposed DMA buffer

    // State tracking
    size_t currentSegment;                               ///< Current segment being processed (0-based)
    size_t totalSegments;                                ///< Total number of segments
    bool isComplete;                                     ///< True when all segments done

    /// @brief Initialize ISR state with frequency and timing
    void init(uint32_t hz, uint32_t t1_ns, uint32_t t2_ns, uint32_t t3_ns,
              uint8_t lanes, size_t bytesPerLaneTotal, size_t segmentSizeBytes);

    /// @brief Reset to start of frame
    void reset();
};

/// @brief Process one segment in ISR context
///
/// This function is designed to be called from an ISR. It processes one
/// segment of LED data, converting it to waveforms and transposing it into
/// the DMA buffer.
///
/// @param state ISR state structure
/// @return true if segment was processed, false if frame complete
///
/// @note This function should be called repeatedly from ISR until it returns false
/// @note Platform code is responsible for starting DMA transmission after each call
bool processNextSegment(ISRState& state);

}  // namespace fl
