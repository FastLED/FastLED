#pragma once

// 2D Perlin noise implementation using s16x16 fixed-point arithmetic
// Extracted from animartrix2_detail.hpp for testing and reuse
//
// LUT-accelerated 2D Perlin noise using s16x16 fixed-point.
// Internals use Q8.24 (24 fractional bits) for precision exceeding float.
// The fade LUT replaces the 6t^5-15t^4+10t^3 polynomial with table lookup.
// The z=0 specialization halves work vs full 3D noise.

#include "fl/fixed_point/s16x16.h"


namespace fl {

struct perlin_s16x16 {
    static constexpr int HP_BITS = 24;
    static constexpr fl::i32 HP_ONE = 1 << HP_BITS; // 16777216 = 1.0

    // Build 257-entry Perlin fade LUT in Q8.24 format.
    static void init_fade_lut(fl::i32 *table);

    // 2D Perlin noise. Input s16x16, output s16x16 approx [-1, 1].
    // perm: 256-byte Perlin permutation table (indexed with & 255).
    static fl::s16x16 pnoise2d(fl::s16x16 fx, fl::s16x16 fy,
                                      const fl::i32 *fade_lut,
                                      const fl::u8 *perm);

    // Raw i32 version: takes s16x16 raw values, returns s16x16 raw value.
    // Avoids from_raw/raw() round-trips when caller already has raw values.
    static fl::i32 pnoise2d_raw(fl::i32 fx_raw, fl::i32 fy_raw,
                                        const fl::i32 *fade_lut,
                                        const fl::u8 *perm);

    static constexpr int FP_BITS = fl::s16x16::FRAC_BITS;
    static constexpr fl::i32 FP_ONE = 1 << FP_BITS;

    // Decompose s16x16 raw value into integer floor and Q8.24 fractional part.
    static void floor_frac(fl::i32 fp16, int &ifloor,
                                                fl::i32 &frac24);

    // LUT fade: 1 lookup + 1 lerp replaces 5 multiplies.
    static fl::i32 fade(fl::i32 t, const fl::i32 *table);

    static fl::i32 lerp(fl::i32 t, fl::i32 a, fl::i32 b);

    // z=0 gradient via branchless coefficient LUT.
    static fl::i32 grad(int hash, fl::i32 x, fl::i32 y);
};

}  // namespace fl
