

#pragma once

// allow-include-after-namespace

#include "fl/stl/align.h"
#include "fl/stl/compiler_control.h"
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
    u8 data;  // Each bit represents one pulse (MSB first)
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

/// @brief Byte-indexed expansion LUT (#2526): 256 entries × 8 bytes = 2 KB.
///
/// Maps a full input byte directly to its 8 Wave8Bit pulse symbols, so the hot
/// expansion is a single indexed 8-byte copy instead of two nibble lookups +
/// two 4-byte copies. Same total memory traffic, ~half the index/issue work —
/// the win on the in-order RV32 core (which is issue-bound here, not bandwidth-
/// bound). Keep this in internal SRAM (not PSRAM) so the lookup stays fast.
struct FL_ALIGNAS(8) Wave8ByteExpansionLut {
    Wave8Byte lut[256]; // byte -> 8 Wave8Bit (8 bytes per byte)
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

/// @brief Build a byte-indexed expansion LUT (#2526) from the nibble LUT.
///
/// Entry b is the concatenation of the high-nibble expansion (symbols[0..3])
/// and the low-nibble expansion (symbols[4..7]) — bit-identical to what
/// wave8_convert_byte_to_wave8byte() produces. Not for ISR use.
Wave8ByteExpansionLut buildWave8ByteExpansionLUT(const Wave8BitExpansionLut &nibble);

// Forward declaration for inline function (implementation in detail/wave8.hpp)
void wave8(u8 lane,
           const Wave8BitExpansionLut &lut,
           u8 (&FL_RESTRICT_PARAM output)[sizeof(Wave8Byte)]);

// Public transposition functions (implementations in wave8.cpp)
void wave8Transpose_2(
    const u8 (&FL_RESTRICT_PARAM lanes)[2],
    const Wave8BitExpansionLut &lut,
    u8 (&FL_RESTRICT_PARAM output)[2 * sizeof(Wave8Byte)]);

void wave8Transpose_4(
    const u8 (&FL_RESTRICT_PARAM lanes)[4],
    const Wave8BitExpansionLut &lut,
    u8 (&FL_RESTRICT_PARAM output)[4 * sizeof(Wave8Byte)]);

void wave8Transpose_8(
    const u8 (&FL_RESTRICT_PARAM lanes)[8],
    const Wave8BitExpansionLut &lut,
    u8 (&FL_RESTRICT_PARAM output)[8 * sizeof(Wave8Byte)]);

void wave8Transpose_16(
    const u8 (&FL_RESTRICT_PARAM lanes)[16],
    const Wave8BitExpansionLut &lut,
    u8 (&FL_RESTRICT_PARAM output)[16 * sizeof(Wave8Byte)]);

// Byte-LUT overloads (#2526): same semantics, faster expansion path.
void wave8Transpose_2(
    const u8 (&FL_RESTRICT_PARAM lanes)[2],
    const Wave8ByteExpansionLut &lut,
    u8 (&FL_RESTRICT_PARAM output)[2 * sizeof(Wave8Byte)]);

void wave8Transpose_4(
    const u8 (&FL_RESTRICT_PARAM lanes)[4],
    const Wave8ByteExpansionLut &lut,
    u8 (&FL_RESTRICT_PARAM output)[4 * sizeof(Wave8Byte)]);

void wave8Transpose_8(
    const u8 (&FL_RESTRICT_PARAM lanes)[8],
    const Wave8ByteExpansionLut &lut,
    u8 (&FL_RESTRICT_PARAM output)[8 * sizeof(Wave8Byte)]);

void wave8Transpose_16(
    const u8 (&FL_RESTRICT_PARAM lanes)[16],
    const Wave8ByteExpansionLut &lut,
    u8 (&FL_RESTRICT_PARAM output)[16 * sizeof(Wave8Byte)]);

// Untranspose functions (for testing - reverse the transpose operation)
void wave8Untranspose_2(
    const u8 (&FL_RESTRICT_PARAM transposed)[2 * sizeof(Wave8Byte)],
    u8 (&FL_RESTRICT_PARAM output)[2 * sizeof(Wave8Byte)]);

void wave8Untranspose_4(
    const u8 (&FL_RESTRICT_PARAM transposed)[4 * sizeof(Wave8Byte)],
    u8 (&FL_RESTRICT_PARAM output)[4 * sizeof(Wave8Byte)]);

void wave8Untranspose_8(
    const u8 (&FL_RESTRICT_PARAM transposed)[8 * sizeof(Wave8Byte)],
    u8 (&FL_RESTRICT_PARAM output)[8 * sizeof(Wave8Byte)]);

void wave8Untranspose_16(
    const u8 (&FL_RESTRICT_PARAM transposed)[16 * sizeof(Wave8Byte)],
    u8 (&FL_RESTRICT_PARAM output)[16 * sizeof(Wave8Byte)]);

} // namespace fl

// Include inline implementations for optimal performance
// This must be after the namespace to avoid the include-after-namespace linter error
