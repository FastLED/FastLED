/// @file wave_transpose.h
/// @brief Waveform generation and transposition for multi-lane LED protocols
///
/// This module provides standalone functions for generating LED waveforms
/// and transposing them into DMA-compatible parallel formats. It is
/// decoupled from PARLIO and DMA hardware for testability.

#pragma once

#include <stddef.h>
#include <stdint.h>
#include "fl/compiler_control.h"

namespace fl {

// ============================================================================
// Wave Pulse Types
// ============================================================================

/// @brief Type-safe container for 8-byte wave pulse pattern
///
/// Represents the pulse expansion of a single bit. Each bit in the LED protocol
/// expands to 8 pulse bytes (e.g., WS2812 bit timing).
///
/// The struct is 8-byte aligned for optimized memory access in ISR/DMA contexts.
struct alignas(8) WavePulses8 {
    uint8_t data[8];
};

// ============================================================================
// Nibble Lookup Table (LUT) Types and Generator
// ============================================================================

/// @brief 8-bit expansion lookup table: 16 nibbles (0x0-0xF), each mapping to 4 WavePulses8 structures
///
/// Each nibble (4 bits) expands to 4 WavePulses8 structures (4 bits × 8 pulses/bit = 32 bytes).
/// This LUT enables branch-free waveform generation by pre-computing all
/// possible nibble expansions for efficient byte-to-waveform conversion.
///
/// The struct is 16-byte aligned for optimized ISR/DMA access patterns.
struct alignas(16) Wave8BitExpansionLut {
    WavePulses8 data[16][4];  // 16 nibbles, each with 4 WavePulses8 (one per bit)
};

#if 0  // thisis wrong so disabled for now
/// @brief Build nibble lookup table for fast byte-to-waveform conversion
///
/// This function pre-computes waveform expansions for all 16 nibble values
/// (0x0-0xF), eliminating conditional branches during waveform generation.
///
/// **LUT Structure:**
/// - Each nibble (4 bits) maps to 4 WavePulses8 structures (4 bits × 8 pulses/bit = 32 bytes)
/// - Bits are processed MSB first: bit3 → bit2 → bit1 → bit0
/// - Each bit expands to one WavePulses8 from bit0_wave or bit1_wave
///
/// **Example:**
/// Nibble 0x6 = 0b0110 (bits 3,2,1,0 = 0,1,1,0)
/// - Bit 3 (0) → WavePulses8 from bit0_wave
/// - Bit 2 (1) → WavePulses8 from bit1_wave
/// - Bit 1 (1) → WavePulses8 from bit1_wave
/// - Bit 0 (0) → WavePulses8 from bit0_wave
///
/// @param bit0_wave Waveform pattern for '0' bit
/// @param bit1_wave Waveform pattern for '1' bit
/// @return Pre-computed 8-bit expansion lookup table
inline Wave8BitExpansionLut buildWaveNibbleLut(
    const WavePulses8& bit0_wave,
    const WavePulses8& bit1_wave
) {
    Wave8BitExpansionLut lut;
    for (int nibble = 0; nibble < 16; nibble++) {
        // Process bits MSB first (bit 3 → bit 2 → bit 1 → bit 0)
        for (int bit_pos = 0; bit_pos < 4; bit_pos++) {
            int bit_mask = (0x8 >> bit_pos);  // 0x8, 0x4, 0x2, 0x1
            bool bit_value = (nibble & bit_mask) != 0;
            const WavePulses8& wave = bit_value ? bit1_wave : bit0_wave;

            // Copy WavePulses8 pattern for this bit
            lut.data[nibble][bit_pos] = wave;
        }
    }
    return lut;
}
#endif  // 0

// ============================================================================
// Waveform Generation and Transposition Functions
// ============================================================================

/// @brief Optimized 2-lane waveform transpose using nibble LUT (branch-free)
///
/// This function uses pre-computed nibble lookup tables to eliminate conditional
/// branches during waveform expansion for maximum performance.
///
/// **Waveform Expansion:**
/// Each input byte (8 bits) is expanded using the pre-computed LUT:
/// - High nibble (bits 7-4) → 32 pulse bytes from lut[high_nibble]
/// - Low nibble (bits 3-0) → 32 pulse bytes from lut[low_nibble]
///
/// This creates 64 pulse bytes per lane (8 bits × 8 pulses/bit).
///
/// **Transposition (data_width=2):**
/// The two 64-byte waveforms are transposed into 16 output bytes.
/// Each output byte packs 4 time ticks, 2 bits per tick (one per lane):
/// - Bit packing: [t0_L0, t0_L1, t1_L0, t1_L1, t2_L0, t2_L1, t3_L0, t3_L1]
///
/// **Performance Benefits:**
/// - No conditional branches (no `if (bit == 0)` checks)
/// - Processes nibbles instead of individual bits (fewer loop iterations)
/// - Direct memory copies from pre-computed LUT
///
/// **Usage Pattern:**
/// @code
/// const uint8_t bit0[8] = {0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00}; // WS2812 bit 0
/// const uint8_t bit1[8] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00}; // WS2812 bit 1
///
/// // Build LUT once (reusable for multiple calls with same timing)
/// Wave8BitExpansionLut lut = buildWaveNibbleLut(bit0, bit1);
///
/// // Use LUT for 2 lanes (fast, no branching)
/// uint8_t lanes[2] = {0xFF, 0x00};
/// uint8_t output[16];
/// waveTranspose8_2(lanes, lut, output);
/// @endcode
///
/// @param lanes Array of 2 lane values (0-255 each)
/// @param lut Pre-computed 8-bit expansion lookup table (from buildWaveNibbleLut)
/// @param output 16-byte output buffer for transposed DMA data
///
/// @see buildWaveNibbleLut() to generate the LUT
FL_IRAM void waveTranspose8_2(
    const uint8_t (&FL_RESTRICT_PARAM lanes)[2],
    const Wave8BitExpansionLut& lut,
    uint8_t (&FL_RESTRICT_PARAM output)[16]
);


} // namespace fl
