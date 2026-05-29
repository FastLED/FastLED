/// @file bit_spread_lut.hpp
/// @brief Shared u32 "spread LUT" bit-matrix transpose primitive (no SIMD, no u64).
///
/// Benchmarked (#2533) on the ESP32-P4 (RV32, in-order): ~2× faster than the
/// unrolled-naive transpose (16-lane 6649→3353 µs = 1.98×; 8-lane 3356→1764 µs
/// = 1.90×; bit-exact). For each output it computes
///   acc = OR over 8 lanes of  spread(laneByte) << lane
/// where `spread()` pre-positions a byte's pulse bits into 8 separate bytes.
/// All ops are native u32 and independent (no dependency chain, no emulated
/// u64) — exactly what the in-order core schedules best — and the tiny table is
/// cache-resident. Used by both wave8 (8 symbols) and wave3 (3 symbols); the
/// per-symbol op is an 8-bit transpose, identical for both.

#pragma once

#include "fl/stl/compiler_control.h"
#include "fl/stl/int.h"

namespace fl {
namespace detail {

/// kSpreadNibble[n] places the 4 bits of nibble n at bit 0 of 4 separate bytes:
/// byte0 = bit3(n), byte1 = bit2(n), byte2 = bit1(n), byte3 = bit0(n).
/// A 16-entry literal table (C++11-valid, no constexpr loop, ~64 bytes, cache-
/// resident). spreadA/spreadB only depend on one nibble each, so this one table
/// serves both halves.
constexpr u32 kSpreadNibble[16] = {
    0x00000000u, 0x01000000u, 0x00010000u, 0x01010000u,
    0x00000100u, 0x01000100u, 0x00010100u, 0x01010100u,
    0x00000001u, 0x01000001u, 0x00010001u, 0x01010001u,
    0x00000101u, 0x01000101u, 0x00010101u, 0x01010101u,
};

/// Pulses 7,6,5,4 of v (byte j = bit (7-j)). Depends only on the high nibble.
FASTLED_FORCE_INLINE u32 spreadA(u8 v) { return kSpreadNibble[v >> 4]; }
/// Pulses 3,2,1,0 of v (byte j = bit (3-j)). Depends only on the low nibble.
FASTLED_FORCE_INLINE u32 spreadB(u8 v) { return kSpreadNibble[v & 0x0Fu]; }

/// Transpose one symbol of 16 lanes (16 input bytes) into 16 output bytes:
/// 8 pulses × 2 bytes, low byte = lanes 0-7, high byte = lanes 8-15, pulse
/// order 7..0 (out[0] = pulse 7 low). Matches the naive layout.
FASTLED_FORCE_INLINE FL_IRAM FL_OPTIMIZE_FUNCTION
void spread_transpose16_symbol(const u8 l[16], u8 out[16]) {
    const u32 aLo = spreadA(l[0]) | spreadA(l[1]) << 1 | spreadA(l[2]) << 2 | spreadA(l[3]) << 3 |
                    spreadA(l[4]) << 4 | spreadA(l[5]) << 5 | spreadA(l[6]) << 6 | spreadA(l[7]) << 7;
    const u32 bLo = spreadB(l[0]) | spreadB(l[1]) << 1 | spreadB(l[2]) << 2 | spreadB(l[3]) << 3 |
                    spreadB(l[4]) << 4 | spreadB(l[5]) << 5 | spreadB(l[6]) << 6 | spreadB(l[7]) << 7;
    const u32 aHi = spreadA(l[8]) | spreadA(l[9]) << 1 | spreadA(l[10]) << 2 | spreadA(l[11]) << 3 |
                    spreadA(l[12]) << 4 | spreadA(l[13]) << 5 | spreadA(l[14]) << 6 | spreadA(l[15]) << 7;
    const u32 bHi = spreadB(l[8]) | spreadB(l[9]) << 1 | spreadB(l[10]) << 2 | spreadB(l[11]) << 3 |
                    spreadB(l[12]) << 4 | spreadB(l[13]) << 5 | spreadB(l[14]) << 6 | spreadB(l[15]) << 7;
    out[0] = static_cast<u8>(aLo);        out[1] = static_cast<u8>(aHi);
    out[2] = static_cast<u8>(aLo >> 8);   out[3] = static_cast<u8>(aHi >> 8);
    out[4] = static_cast<u8>(aLo >> 16);  out[5] = static_cast<u8>(aHi >> 16);
    out[6] = static_cast<u8>(aLo >> 24);  out[7] = static_cast<u8>(aHi >> 24);
    out[8] = static_cast<u8>(bLo);        out[9] = static_cast<u8>(bHi);
    out[10] = static_cast<u8>(bLo >> 8);  out[11] = static_cast<u8>(bHi >> 8);
    out[12] = static_cast<u8>(bLo >> 16); out[13] = static_cast<u8>(bHi >> 16);
    out[14] = static_cast<u8>(bLo >> 24); out[15] = static_cast<u8>(bHi >> 24);
}

/// Transpose one symbol of 8 lanes (8 input bytes) into 8 output bytes:
/// 8 pulses × 1 byte (bit L = lane L), pulse order 7..0 (out[0] = pulse 7).
FASTLED_FORCE_INLINE FL_IRAM FL_OPTIMIZE_FUNCTION
void spread_transpose8_symbol(const u8 l[8], u8 out[8]) {
    const u32 a = spreadA(l[0]) | spreadA(l[1]) << 1 | spreadA(l[2]) << 2 | spreadA(l[3]) << 3 |
                  spreadA(l[4]) << 4 | spreadA(l[5]) << 5 | spreadA(l[6]) << 6 | spreadA(l[7]) << 7;
    const u32 b = spreadB(l[0]) | spreadB(l[1]) << 1 | spreadB(l[2]) << 2 | spreadB(l[3]) << 3 |
                  spreadB(l[4]) << 4 | spreadB(l[5]) << 5 | spreadB(l[6]) << 6 | spreadB(l[7]) << 7;
    out[0] = static_cast<u8>(a);        out[1] = static_cast<u8>(a >> 8);
    out[2] = static_cast<u8>(a >> 16);  out[3] = static_cast<u8>(a >> 24);
    out[4] = static_cast<u8>(b);        out[5] = static_cast<u8>(b >> 8);
    out[6] = static_cast<u8>(b >> 16);  out[7] = static_cast<u8>(b >> 24);
}

}  // namespace detail
}  // namespace fl
