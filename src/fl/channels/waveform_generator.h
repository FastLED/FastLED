/// @file waveform_generator.h
/// @brief Generic waveform generator for clockless LED protocols
///
/// This module provides platform-agnostic waveform generation for driving
/// clockless LED protocols (WS2812, etc.) via hardware peripherals (DMA/SPI/RMT/PARLIO).
///
/// # Design Overview
///
/// The waveform generator solves the problem of converting LED timing requirements
/// (T1, T2, T3) into pulse sequences that can be sent to hardware peripherals.
/// The hardware-specific driver is responsible for transposing/formatting these
/// waveforms into the required format (e.g., bit-packing for PARLIO, byte-interleaving
/// for SPI).
///
/// ## Algorithm
///
/// 1. **Resolution Calculation**
///    - Based on hardware frequency (Hz), calculate nanoseconds per pulse
///    - Each bit is encoded as N pulses (ceiling division of timing / resolution)
///    - Example: 3.2 MHz clock → 312.5ns per pulse, WS2812 → 4 pulses per bit
///
/// 2. **Bit-to-Waveform Mapping**
///    - Bit 0: HIGH for T1 pulses, then LOW for (T2+T3) pulses
///    - Bit 1: HIGH for (T1+T2) pulses, then LOW for T3 pulses
///    - Each pulse is represented as a byte (0xFF=HIGH, 0x00=LOW)
///
/// 3. **Byte Expansion**
///    - Each LED data byte (8 bits) expands to 8 × N pulse bytes
///    - Uses precomputed bit0/bit1 waveform patterns for efficiency
///    - Output is a sequence of 0xFF/0x00 bytes representing the waveform
///
/// ## Separation of Concerns
///
/// **Waveform Generator** (this module):
/// - Generate bit0/bit1 waveform patterns from timing parameters
/// - Expand data bytes to waveform sequences
/// - Platform-agnostic, no hardware-specific formatting
///
/// **Hardware Driver** (e.g., PARLIO, SPI, RMT):
/// - Take waveforms from generator
/// - Transpose/format into hardware-specific layout
/// - Queue to DMA and manage transmission
///
/// @note This module does NOT perform transposition or hardware-specific formatting.
///       Those operations belong in the downstream hardware driver.

#pragma once

#include "ftl/stdint.h"
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
/// @param b0_waveform_size Size of b0_waveform buffer in bytes
/// @return Number of bytes written to b0_waveform
size_t generateBit0Waveform(
    uint32_t hz,
    uint32_t t1_ns,
    uint32_t t2_ns,
    uint32_t t3_ns,
    uint8_t* b0_waveform,
    size_t b0_waveform_size
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
/// @param b1_waveform_size Size of b1_waveform buffer in bytes
/// @return Number of bytes written to b1_waveform
size_t generateBit1Waveform(
    uint32_t hz,
    uint32_t t1_ns,
    uint32_t t2_ns,
    uint32_t t3_ns,
    uint8_t* b1_waveform,
    size_t b1_waveform_size
);

/// @brief Convert byte to waveform patterns (8 bits → 8 waveforms)
///
/// Takes a single byte and generates 8 waveform patterns (one per bit).
/// Each bit is expanded using the provided precomputed waveforms.
///
/// @param dataByte Input byte (8 bits of LED data)
/// @param b0_waveform Precomputed waveform for bit 0
/// @param b0_waveform_size Size of b0_waveform in bytes
/// @param b1_waveform Precomputed waveform for bit 1
/// @param b1_waveform_size Size of b1_waveform in bytes
/// @param output Output buffer (size must be >= 8 * pulsesPerBit)
/// @param output_size Size of output buffer in bytes
/// @return Number of bytes written
///
/// @note Both b0_waveform and b1_waveform must be the same size (pulsesPerBit)
size_t expandByteToWaveforms(
    uint8_t dataByte,
    const uint8_t* b0_waveform,
    size_t b0_waveform_size,
    const uint8_t* b1_waveform,
    size_t b1_waveform_size,
    uint8_t* output,
    size_t output_size
);

}  // namespace fl
