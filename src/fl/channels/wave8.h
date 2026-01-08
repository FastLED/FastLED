

#pragma once

// allow-include-after-namespace

#include "fl/align.h"
#include "fl/compiler_control.h"
#include "fl/stl/stdint.h"

namespace fl {

struct ChipsetTiming;

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
struct FL_ALIGNAS(8) Wave8Byte {
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
struct FL_ALIGNAS(8) Wave8BitExpansionLut {
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

// Forward declaration for inline function (implementation in detail/wave8.hpp)
void wave8(uint8_t lane,
           const Wave8BitExpansionLut &lut,
           uint8_t (&FL_RESTRICT_PARAM output)[sizeof(Wave8Byte)]);

// Public transposition functions (implementations in wave8.cpp)
void wave8Transpose_2(
    const uint8_t (&FL_RESTRICT_PARAM lanes)[2],
    const Wave8BitExpansionLut &lut,
    uint8_t (&FL_RESTRICT_PARAM output)[2 * sizeof(Wave8Byte)]);

void wave8Transpose_4(
    const uint8_t (&FL_RESTRICT_PARAM lanes)[4],
    const Wave8BitExpansionLut &lut,
    uint8_t (&FL_RESTRICT_PARAM output)[4 * sizeof(Wave8Byte)]);

void wave8Transpose_8(
    const uint8_t (&FL_RESTRICT_PARAM lanes)[8],
    const Wave8BitExpansionLut &lut,
    uint8_t (&FL_RESTRICT_PARAM output)[8 * sizeof(Wave8Byte)]);

void wave8Transpose_16(
    const uint8_t (&FL_RESTRICT_PARAM lanes)[16],
    const Wave8BitExpansionLut &lut,
    uint8_t (&FL_RESTRICT_PARAM output)[16 * sizeof(Wave8Byte)]);

// Untranspose functions (for testing - reverse the transpose operation)
void wave8Untranspose_2(
    const uint8_t (&FL_RESTRICT_PARAM transposed)[2 * sizeof(Wave8Byte)],
    uint8_t (&FL_RESTRICT_PARAM output)[2 * sizeof(Wave8Byte)]);

void wave8Untranspose_4(
    const uint8_t (&FL_RESTRICT_PARAM transposed)[4 * sizeof(Wave8Byte)],
    uint8_t (&FL_RESTRICT_PARAM output)[4 * sizeof(Wave8Byte)]);

void wave8Untranspose_8(
    const uint8_t (&FL_RESTRICT_PARAM transposed)[8 * sizeof(Wave8Byte)],
    uint8_t (&FL_RESTRICT_PARAM output)[8 * sizeof(Wave8Byte)]);

void wave8Untranspose_16(
    const uint8_t (&FL_RESTRICT_PARAM transposed)[16 * sizeof(Wave8Byte)],
    uint8_t (&FL_RESTRICT_PARAM output)[16 * sizeof(Wave8Byte)]);

} // namespace fl

// Include inline implementations for optimal performance
// This must be after the namespace to avoid the include-after-namespace linter error
