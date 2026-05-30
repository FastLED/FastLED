

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

/// @brief Pipe2: transpose 16-lane × 2-byte-positions (#2548).
///        Bit-identical to two sequential `wave8Transpose_16` calls. Internally
///        interleaves the two independent OR-trees inside the symbol loop so
///        the in-order RV32 P4 can fill load-use stalls from position A with
///        ALU ops from position B. Measured 9655 → 7625 µs/frame (+26%) on
///        P4 v1.3 (16-lane × 256-LED, byte-LUT path) — first variant to beat
///        the 7680 µs WS2812B 16-lane TX target.
void wave8Transpose_16x2_pipe2(
    const u8 (&FL_RESTRICT_PARAM lanes_a)[16],
    const u8 (&FL_RESTRICT_PARAM lanes_b)[16],
    const Wave8ByteExpansionLut &lut,
    u8 (&FL_RESTRICT_PARAM output_a)[16 * sizeof(Wave8Byte)],
    u8 (&FL_RESTRICT_PARAM output_b)[16 * sizeof(Wave8Byte)]);

/// @brief Pipe4: transpose 16-lane × 4-byte-positions (#2548).
///        Bit-identical to four sequential `wave8Transpose_16` calls. Peak of
///        the cross-position ILP curve on RV32 P4 — pipe2 = +26%, pipe3 = +36%,
///        **pipe4 = +41%** vs baseline (9 651 → 6 822 µs/frame). pipe6 / pipe8
///        regress to 94% (32-GPR budget exceeded → compiler spills).
///        Measured **11% UNDER the 7 680 µs WS2812B TX target** — comfortable
///        margin for ISR-chunked streaming.
void wave8Transpose_16x4_pipe4(
    const u8 (&FL_RESTRICT_PARAM lanes_a)[16],
    const u8 (&FL_RESTRICT_PARAM lanes_b)[16],
    const u8 (&FL_RESTRICT_PARAM lanes_c)[16],
    const u8 (&FL_RESTRICT_PARAM lanes_d)[16],
    const Wave8ByteExpansionLut &lut,
    u8 (&FL_RESTRICT_PARAM output_a)[16 * sizeof(Wave8Byte)],
    u8 (&FL_RESTRICT_PARAM output_b)[16 * sizeof(Wave8Byte)],
    u8 (&FL_RESTRICT_PARAM output_c)[16 * sizeof(Wave8Byte)],
    u8 (&FL_RESTRICT_PARAM output_d)[16 * sizeof(Wave8Byte)]);

/// @brief BF1 for 2-lane Wave8 (#2548 deep-dive followup).
void wave8Transpose_2_bf1(
    const u8 (&FL_RESTRICT_PARAM lanes)[2],
    const Wave8ByteExpansionLut &lut,
    u8 (&FL_RESTRICT_PARAM output)[2 * sizeof(Wave8Byte)]);

/// @brief BF1 for 4-lane Wave8 (#2548 deep-dive followup).
void wave8Transpose_4_bf1(
    const u8 (&FL_RESTRICT_PARAM lanes)[4],
    const Wave8ByteExpansionLut &lut,
    u8 (&FL_RESTRICT_PARAM output)[4 * sizeof(Wave8Byte)]);

/// @brief BF1 for 8-lane Wave8 (#2548 deep-dive followup).
void wave8Transpose_8_bf1(
    const u8 (&FL_RESTRICT_PARAM lanes)[8],
    const Wave8ByteExpansionLut &lut,
    u8 (&FL_RESTRICT_PARAM output)[8 * sizeof(Wave8Byte)]);

/// @brief BF1: chipset-aware direct encode for 16-lane Wave8 (#2548 deep-dive).
///        Bypasses the byte_lut. Uses the algebraic identity
///        `output_bit(s, p, lane) = M0_p XOR (input_bit_(7-s) AND D_p)` where
///        W0/W1 are the bit-0/bit-1 waveform patterns extracted from the lut.
///        Bit-identical to wave8Transpose_16 for ANY Wave8 chipset. The
///        bit-transpose of input bytes is computed ONCE (not 8× per symbol).
///        Measured **9 651 → 4 465 µs/frame (2.16×)** vs baseline on P4 v1.3.
void wave8Transpose_16_bf1(
    const u8 (&FL_RESTRICT_PARAM lanes)[16],
    const Wave8ByteExpansionLut &lut,
    u8 (&FL_RESTRICT_PARAM output)[16 * sizeof(Wave8Byte)]);

/// @brief BF1 + pipe4: 4-position-pipelined direct encode (#2548 deep-dive).
///        Combines BF1's algorithmic reduction with pipe4 cross-position ILP.
///        Empirical peak of all prototypes:
///        **9 651 → 1 757 µs/frame = 5.49× speedup** on P4 v1.3 16-lane × 256
///        LEDs. **4.4× faster than the 7 680 µs WS2812B 16-lane TX target** —
///        encode now has massive headroom for ISR-driven chunked streaming.
void wave8Transpose_16x4_bf1_pipe4(
    const u8 (&FL_RESTRICT_PARAM lanes_a)[16],
    const u8 (&FL_RESTRICT_PARAM lanes_b)[16],
    const u8 (&FL_RESTRICT_PARAM lanes_c)[16],
    const u8 (&FL_RESTRICT_PARAM lanes_d)[16],
    const Wave8ByteExpansionLut &lut,
    u8 (&FL_RESTRICT_PARAM output_a)[16 * sizeof(Wave8Byte)],
    u8 (&FL_RESTRICT_PARAM output_b)[16 * sizeof(Wave8Byte)],
    u8 (&FL_RESTRICT_PARAM output_c)[16 * sizeof(Wave8Byte)],
    u8 (&FL_RESTRICT_PARAM output_d)[16 * sizeof(Wave8Byte)]);

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
