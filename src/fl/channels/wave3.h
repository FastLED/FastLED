

#pragma once

// allow-include-after-namespace

#include "fl/align.h"
#include "fl/compiler_control.h"
#include "fl/stl/stdint.h"

namespace fl {

struct ChipsetTiming;

/// @brief Type-safe container for 3-byte wave pulse pattern (wave3 encoding)
///
/// Represents the pulse expansion of a single LED byte in wave3 format.
/// Each LED bit expands to 3 clock ticks, so 8 bits = 24 ticks = 3 bytes.
/// High nibble (12 bits) + low nibble (12 bits) = 24 bits = 3 bytes.
struct Wave3Byte {
    u8 data[3]; // 24 bits total (8 LED bits × 3 ticks each)
};

/// @brief Lookup table for nibble-to-waveform expansion in wave3 format (32 bytes)
///
/// Maps each 4-bit nibble (0x0 to 0xF) to a 12-bit waveform pattern stored in u16.
/// Each LED bit expands to 3 ticks: bit-0 → 100 (1H,2L), bit-1 → 110 (2H,1L).
/// 4 bits × 3 ticks = 12 bits per nibble, stored in the lower 12 bits of u16.
///
/// Total size: 16 nibbles × 2 bytes = 32 bytes
struct FL_ALIGNAS(4) Wave3BitExpansionLut {
    u16 lut[16]; // nibble → 12-bit pattern in lower bits of u16
};

/// @brief Check if a chipset timing is eligible for wave3 encoding
///
/// Wave3 requires that normalized T0H and T1H round to exactly 1 and 2 ticks
/// (or 2 and 1) out of 3, and both are in [1,2].
///
/// @param timing ChipsetTiming struct with T1, T2, T3 in nanoseconds
/// @return true if wave3 encoding is possible for this chipset
bool canUseWave3(const ChipsetTiming& timing);

/// @brief Calculate the clock frequency for wave3 encoding
///
/// Clock = 3,000,000,000 / total_period_ns, so that 3 ticks = 1 bit period.
///
/// @param timing ChipsetTiming struct with T1, T2, T3 in nanoseconds
/// @return Clock frequency in Hz (e.g., 2400000 for WS2812)
u32 wave3ClockFrequencyHz(const ChipsetTiming& timing);

/// @brief Build a Wave3BitExpansionLut from chipset timing data
///
/// Converts three-phase LED timing (T1, T2, T3) into a nibble lookup table
/// for 3-tick-per-bit waveform expansion. Each nibble maps to 12 bits.
///
/// @param timing ChipsetTiming struct containing T1, T2, T3 in nanoseconds
/// @return Populated Wave3BitExpansionLut lookup table (32 bytes)
Wave3BitExpansionLut buildWave3ExpansionLUT(const ChipsetTiming& timing);

// Forward declaration for inline function (implementation in detail/wave3.hpp)
void wave3(u8 lane,
           const Wave3BitExpansionLut& lut,
           u8 (&FL_RESTRICT_PARAM output)[sizeof(Wave3Byte)]);

// Public transposition functions (implementations in wave3.cpp.hpp)
void wave3Transpose_2(
    const u8 (&FL_RESTRICT_PARAM lanes)[2],
    const Wave3BitExpansionLut& lut,
    u8 (&FL_RESTRICT_PARAM output)[2 * sizeof(Wave3Byte)]);

void wave3Transpose_4(
    const u8 (&FL_RESTRICT_PARAM lanes)[4],
    const Wave3BitExpansionLut& lut,
    u8 (&FL_RESTRICT_PARAM output)[4 * sizeof(Wave3Byte)]);

void wave3Transpose_8(
    const u8 (&FL_RESTRICT_PARAM lanes)[8],
    const Wave3BitExpansionLut& lut,
    u8 (&FL_RESTRICT_PARAM output)[8 * sizeof(Wave3Byte)]);

void wave3Transpose_16(
    const u8 (&FL_RESTRICT_PARAM lanes)[16],
    const Wave3BitExpansionLut& lut,
    u8 (&FL_RESTRICT_PARAM output)[16 * sizeof(Wave3Byte)]);

// Untranspose functions (for testing - reverse the transpose operation)
void wave3Untranspose_2(
    const u8 (&FL_RESTRICT_PARAM transposed)[2 * sizeof(Wave3Byte)],
    u8 (&FL_RESTRICT_PARAM output)[2 * sizeof(Wave3Byte)]);

void wave3Untranspose_4(
    const u8 (&FL_RESTRICT_PARAM transposed)[4 * sizeof(Wave3Byte)],
    u8 (&FL_RESTRICT_PARAM output)[4 * sizeof(Wave3Byte)]);

void wave3Untranspose_8(
    const u8 (&FL_RESTRICT_PARAM transposed)[8 * sizeof(Wave3Byte)],
    u8 (&FL_RESTRICT_PARAM output)[8 * sizeof(Wave3Byte)]);

void wave3Untranspose_16(
    const u8 (&FL_RESTRICT_PARAM transposed)[16 * sizeof(Wave3Byte)],
    u8 (&FL_RESTRICT_PARAM output)[16 * sizeof(Wave3Byte)]);

} // namespace fl

// Include inline implementations for optimal performance
// This must be after the namespace to avoid the include-after-namespace linter error
