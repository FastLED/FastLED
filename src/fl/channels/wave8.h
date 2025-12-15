

#pragma once

#include "fl/chipsets/led_timing.h"
#include "fl/compiler_control.h"
#include "ftl/cstddef.h"
#include "ftl/stdint.h"

namespace fl {

/// @brief Type-safe container for 8-byte wave pulse pattern
///
/// Represents the pulse expansion of a single bit. Each bit in the LED protocol
/// expands to 8 pulse bytes (e.g., WS2812 bit timing).
///
/// The struct is 8-byte aligned for optimized memory access in ISR/DMA
/// contexts.
struct alignas(8) Wave8Bit {
    uint8_t data[8];
};

struct alignas(8) Wave8Byte {
    Wave8Bit symbols[8];
};

// ============================================================================
// Nibble Lookup Table (LUT) Types and Generator
// ============================================================================

/// @brief Lookup table for nibble-to-waveform expansion
///
/// Maps each 4-bit nibble (0x0 to 0xF) to 4 Wave8Bit structures.
/// This reduces byte conversion from 8 lookups (bit-level) to 2 lookups
/// (nibble-level).
struct alignas(8) Wave8BitExpansionLut {
    Wave8Bit lut[16][4]; // nibble -> 4 Wave8Bit (half symbol)
};

/// @brief Build a Wave8BitExpansionLut from chipset timing data
///
/// Converts three-phase LED timing (T1, T2, T3) into a nibble lookup table
/// for 8-pulse-per-bit waveform expansion. The timing is normalized to a
/// period of 1.0 and mapped to 8 pulse bytes per bit.
///
/// @param timing ChipsetTiming struct containing T1, T2, T3 in nanoseconds
/// @return Populated Wave8BitExpansionLut lookup table
Wave8BitExpansionLut buildWave8ExpansionLUT(const ChipsetTiming &timing);

FL_IRAM void wave8(
    uint8_t lane,
    const Wave8BitExpansionLut &lut,
    uint8_t (&FL_RESTRICT_PARAM output)[sizeof(Wave8Byte)]);

FL_IRAM void waveTranspose8_2(
    const uint8_t (&FL_RESTRICT_PARAM lanes)[2],
    const Wave8BitExpansionLut &lut,
    uint8_t (&FL_RESTRICT_PARAM output)[2 * sizeof(Wave8Byte)]);

} // namespace fl
