

#pragma once

#include "fl/chipsets/led_timing.h"
#include "fl/compiler_control.h"
#include "fl/force_inline.h"
#include "fl/stl/cstddef.h"
#include "fl/stl/stdint.h"

namespace fl {

/// @brief Type-safe container for packed 8-bit wave pulse pattern
///
/// Represents the pulse expansion of a single bit in packed format.
/// Each bit in the LED protocol expands to 8 pulses, stored as 8 bits
/// in a single byte (MSB = first pulse).
///
/// Example: 0b11000000 (0xC0) = 2 HIGH pulses, 6 LOW pulses
struct Wave8Bit {
    uint8_t data;  // Each bit represents one pulse (MSB first)
};

/// @brief Container for 8 packed wave symbols (8 bytes total)
///
/// Holds 8 Wave8Bit structures (1 byte each = 8 bytes total).
/// The struct is 8-byte aligned for optimized memory access.
struct alignas(8) Wave8Byte {
    Wave8Bit symbols[8];  // 8 bytes total (8 symbols × 1 byte each)
};

// ============================================================================
// Nibble Lookup Table (LUT) Types and Generator
// ============================================================================

/// @brief Lookup table for nibble-to-waveform expansion (64 bytes total)
///
/// Maps each 4-bit nibble (0x0 to 0xF) to 4 Wave8Bit structures (4 bytes).
/// This reduces byte conversion from 8 lookups (bit-level) to 2 lookups
/// (nibble-level).
///
/// Total size: 16 nibbles × 4 Wave8Bit × 1 byte = 64 bytes
struct alignas(8) Wave8BitExpansionLut {
    Wave8Bit lut[16][4]; // nibble -> 4 Wave8Bit (4 bytes per nibble)
};

/// @brief Build a Wave8BitExpansionLut from chipset timing data
///
/// Converts three-phase LED timing (T1, T2, T3) into a nibble lookup table
/// for 8-pulse-per-bit waveform expansion. The timing is normalized to a
/// period of 1.0 and mapped to 8 pulses per bit (packed into 1 byte).
///
/// Never call this from ISR handlers. This is something that should be
/// called before entering ISR context.
///
/// @param timing ChipsetTiming struct containing T1, T2, T3 in nanoseconds
/// @return Populated Wave8BitExpansionLut lookup table (64 bytes)
Wave8BitExpansionLut buildWave8ExpansionLUT(const ChipsetTiming &timing);

/// @brief Helper: Convert byte to Wave8Byte using nibble LUT (internal use only)
/// @note Inline implementation for ISR performance
FL_OPTIMIZE_FUNCTION FL_IRAM FASTLED_FORCE_INLINE
static void convertByteToWave8Byte_inline(uint8_t byte_value,
                                           const Wave8BitExpansionLut &lut,
                                           Wave8Byte *output) {
    // Cache pointer to high nibble data to avoid repeated indexing
    const Wave8Bit *high_nibble_data = lut.lut[(byte_value >> 4) & 0xF];
    for (int i = 0; i < 4; i++) {
        output->symbols[i] = high_nibble_data[i];
    }

    // Cache pointer to low nibble data to avoid repeated indexing
    const Wave8Bit *low_nibble_data = lut.lut[byte_value & 0xF];
    for (int i = 0; i < 4; i++) {
        output->symbols[4 + i] = low_nibble_data[i];
    }
}

/// @brief Convert byte to 8 Wave8Bit structures using nibble LUT
/// @note Inline implementation for ISR performance (always_inline requires visible body)
FL_OPTIMIZE_FUNCTION FL_IRAM FASTLED_FORCE_INLINE
void wave8(uint8_t lane,
           const Wave8BitExpansionLut &lut,
           uint8_t (&FL_RESTRICT_PARAM output)[sizeof(Wave8Byte)]) {
    // Convert single lane byte to wave pulse symbols (8 bytes packed)
    // Use properly aligned local variable to avoid alignment issues
    Wave8Byte waveformSymbol;
    convertByteToWave8Byte_inline(lane, lut, &waveformSymbol);

    // Copy to output array byte-by-byte (sizeof(Wave8Byte) = 8)
    const uint8_t* src = &waveformSymbol.symbols[0].data;
    for (size_t i = 0; i < sizeof(Wave8Byte); i++) {
        output[i] = src[i];
    }
}

FL_IRAM void wave8Transpose_2(
    const uint8_t (&FL_RESTRICT_PARAM lanes)[2],
    const Wave8BitExpansionLut &lut,
    uint8_t (&FL_RESTRICT_PARAM output)[2 * sizeof(Wave8Byte)]);

} // namespace fl
