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
// Nibble Lookup Table (LUT) Types and Generator
// ============================================================================

/// @brief Nibble lookup table: 16 nibbles (0x0-0xF), each mapping to 32 pulse bytes
///
/// Each nibble (4 bits) expands to 32 pulse bytes (4 bits × 8 pulses/bit).
/// This LUT enables branch-free waveform generation by pre-computing all
/// possible nibble expansions.
typedef uint8_t wave_nibble_lut_t[16][32];

/// @brief Build nibble lookup table for fast byte-to-waveform conversion
///
/// This function pre-computes waveform expansions for all 16 nibble values
/// (0x0-0xF), eliminating conditional branches during waveform generation.
///
/// **LUT Structure:**
/// - Each nibble (4 bits) maps to 32 pulse bytes (4 bits × 8 pulses/bit)
/// - Bits are processed MSB first: bit3 → bit2 → bit1 → bit0
/// - Each bit expands to 8 pulse bytes from bit0_wave or bit1_wave
///
/// **Example:**
/// Nibble 0x6 = 0b0110 (bits 3,2,1,0 = 0,1,1,0)
/// - Bit 3 (0) → 8 bytes from bit0_wave
/// - Bit 2 (1) → 8 bytes from bit1_wave
/// - Bit 1 (1) → 8 bytes from bit1_wave
/// - Bit 0 (0) → 8 bytes from bit0_wave
///
/// @param lut Output array [16][32] to fill with pulse bytes
/// @param bit0_wave 8-byte waveform pattern for '0' bit
/// @param bit1_wave 8-byte waveform pattern for '1' bit
inline void buildWaveNibbleLut(
    wave_nibble_lut_t& lut,
    const uint8_t bit0_wave[8],
    const uint8_t bit1_wave[8]
) {
    for (int nibble = 0; nibble < 16; nibble++) {
        // Process bits MSB first (bit 3 → bit 2 → bit 1 → bit 0)
        for (int bit_pos = 0; bit_pos < 4; bit_pos++) {
            int bit_mask = (0x8 >> bit_pos);  // 0x8, 0x4, 0x2, 0x1
            bool bit_value = (nibble & bit_mask) != 0;
            const uint8_t* wave = bit_value ? bit1_wave : bit0_wave;

            // Copy 8-byte waveform pattern for this bit
            size_t dest_offset = bit_pos * 8;
            for (size_t i = 0; i < 8; i++) {
                lut[nibble][dest_offset + i] = wave[i];
            }
        }
    }
}

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
/// wave_nibble_lut_t lut;
/// buildWaveNibbleLut(lut, bit0, bit1);
///
/// // Use LUT for multiple byte pairs (fast, no branching)
/// uint8_t output[16];
/// waveTranspose8_2_simple(0xFF, 0x00, lut, output);
/// waveTranspose8_2_simple(0xAA, 0x55, lut, output2);
/// @endcode
///
/// @param laneA Byte value for lane 0 (0-255)
/// @param laneB Byte value for lane 1 (0-255)
/// @param lut Pre-computed nibble lookup table (from buildWaveNibbleLut)
/// @param output 16-byte output buffer for transposed DMA data
///
/// @see buildWaveNibbleLut() to generate the LUT
FL_IRAM void waveTranspose8_2_simple(
    uint8_t laneA,
    uint8_t laneB,
    const wave_nibble_lut_t& lut,
    uint8_t output[16]
);

/// @brief Optimized 4-lane waveform transpose using nibble LUT (branch-free)
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
/// **Transposition (data_width=4):**
/// The four 64-byte waveforms are transposed into 128 output bytes.
/// Each output byte packs 2 time ticks, 4 bits per tick (one per lane):
/// - Bit packing: [t0_L0, t0_L1, t0_L2, t0_L3, t1_L0, t1_L1, t1_L2, t1_L3]
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
/// wave_nibble_lut_t lut;
/// buildWaveNibbleLut(lut, bit0, bit1);
///
/// // Use LUT for multiple byte quads (fast, no branching)
/// uint8_t output[128];
/// waveTranspose8_4(0xFF, 0x00, 0xAA, 0x55, lut, output);
/// @endcode
///
/// @param laneA Byte value for lane 0 (0-255)
/// @param laneB Byte value for lane 1 (0-255)
/// @param laneC Byte value for lane 2 (0-255)
/// @param laneD Byte value for lane 3 (0-255)
/// @param lut Pre-computed nibble lookup table (from buildWaveNibbleLut)
/// @param output 128-byte output buffer for transposed DMA data
///
/// @see buildWaveNibbleLut() to generate the LUT
FL_IRAM void waveTranspose8_4(
    uint8_t laneA,
    uint8_t laneB,
    uint8_t laneC,
    uint8_t laneD,
    const wave_nibble_lut_t& lut,
    uint8_t output[128]
);

/// @brief Optimized 8-lane waveform transpose using nibble LUT (branch-free)
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
/// **Transposition (data_width=8):**
/// The eight 64-byte waveforms are transposed into 64 output bytes.
/// Each output byte packs 1 time tick with all 8 lane bits:
/// - Bit packing: [t0_L0, t0_L1, t0_L2, t0_L3, t0_L4, t0_L5, t0_L6, t0_L7]
/// - Special case: 8 lanes fills all 8 bits in one tick (1 tick per byte)
/// - 64 pulses × 1 byte/tick = 64 output bytes
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
/// wave_nibble_lut_t lut;
/// buildWaveNibbleLut(lut, bit0, bit1);
///
/// // Use LUT for multiple byte sets (fast, no branching)
/// uint8_t output[64];
/// waveTranspose8_8(0xFF, 0x00, 0xAA, 0x55, 0xF0, 0x0F, 0x33, 0xCC, lut, output);
/// @endcode
///
/// @param lane0 Byte value for lane 0 (0-255)
/// @param lane1 Byte value for lane 1 (0-255)
/// @param lane2 Byte value for lane 2 (0-255)
/// @param lane3 Byte value for lane 3 (0-255)
/// @param lane4 Byte value for lane 4 (0-255)
/// @param lane5 Byte value for lane 5 (0-255)
/// @param lane6 Byte value for lane 6 (0-255)
/// @param lane7 Byte value for lane 7 (0-255)
/// @param lut Pre-computed nibble lookup table (from buildWaveNibbleLut)
/// @param output 64-byte output buffer for transposed DMA data
///
/// @see buildWaveNibbleLut() to generate the LUT
FL_IRAM void waveTranspose8_8(
    uint8_t lane0, uint8_t lane1, uint8_t lane2, uint8_t lane3,
    uint8_t lane4, uint8_t lane5, uint8_t lane6, uint8_t lane7,
    const wave_nibble_lut_t& lut,
    uint8_t output[64]
);


/// @brief Optimized 16-lane waveform transpose using nibble LUT (branch-free)
///
/// This function uses pre-computed nibble lookup tables to eliminate conditional
/// branches during waveform expansion for maximum performance.
///
/// **Waveform Expansion:**
/// Each input byte (8 bits) is expanded using the pre-computed LUT:
/// - High nibble (bits 7-4) -> 32 pulse bytes from lut[high_nibble]
/// - Low nibble (bits 3-0) -> 32 pulse bytes from lut[low_nibble]
///
/// This creates 64 pulse bytes per lane (8 bits x 8 pulses/bit).
///
/// **Transposition (data_width=16):**
/// The 16 waveforms (16 x 64 bytes = 1024 pulse bytes) are transposed into 128 output bytes.
/// Each pair of output bytes represents one time tick across all 16 lanes:
/// - Byte 0: [t0_L0-L7], Byte 1: [t0_L8-L15]
/// - Byte 2: [t1_L0-L7], Byte 3: [t1_L8-L15]
/// - ...
/// - 64 pulses x 2 bytes/tick = 128 output bytes
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
/// wave_nibble_lut_t lut;
/// buildWaveNibbleLut(lut, bit0, bit1);
///
/// // Use LUT for 16 lanes (fast, no branching)
/// uint8_t lanes[16] = {0xFF, 0x00, 0xAA, 0x55, ...};
/// uint8_t output[128];
/// waveTranspose8_16(lanes, lut, output);
/// @endcode
///
/// @param lanes Array of 16 lane values (0-255 each)
/// @param lut Pre-computed nibble lookup table (from buildWaveNibbleLut)
/// @param output 128-byte output buffer for transposed DMA data
///
/// @see buildWaveNibbleLut() to generate the LUT
FL_IRAM void waveTranspose8_16(
    const uint8_t lanes[16],
    const wave_nibble_lut_t& lut,
    uint8_t output[128]
);

} // namespace fl
