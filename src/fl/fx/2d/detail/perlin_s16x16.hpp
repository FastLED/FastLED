#pragma once

// 2D Perlin noise implementation using s16x16 fixed-point arithmetic
// Extracted from animartrix2_detail.hpp for testing and reuse
//
// LUT-accelerated 2D Perlin noise using s16x16 fixed-point.
// Internals use Q8.24 (24 fractional bits) for precision exceeding float.
// The fade LUT replaces the 6t^5-15t^4+10t^3 polynomial with table lookup.
// The z=0 specialization halves work vs full 3D noise.

#include "fl/fixed_point/s16x16.h"
#include "fl/force_inline.h"

namespace fl {
namespace fx {

struct perlin_s16x16 {
    static constexpr int HP_BITS = 24;
    static constexpr fl::i32 HP_ONE = 1 << HP_BITS; // 16777216 = 1.0

    // Build 257-entry Perlin fade LUT in Q8.24 format.
    static inline void init_fade_lut(fl::i32 *table) {
        for (int i = 0; i <= 256; i++) {
            fl::i64 t = static_cast<fl::i64>(i) * (HP_ONE / 256);
            fl::i64 t2 = (t * t) >> HP_BITS;
            fl::i64 t3 = (t2 * t) >> HP_BITS;
            fl::i64 inner = (t * (6LL * HP_ONE)) >> HP_BITS;
            inner -= 15LL * HP_ONE;
            inner = (t * inner) >> HP_BITS;
            inner += 10LL * HP_ONE;
            table[i] = static_cast<fl::i32>((t3 * inner) >> HP_BITS);
        }
    }

    // 2D Perlin noise. Input s16x16, output s16x16 approx [-1, 1].
    // perm: 256-byte Perlin permutation table (indexed with & 255).
    static inline fl::s16x16 pnoise2d(fl::s16x16 fx, fl::s16x16 fy,
                                      const fl::i32 *fade_lut,
                                      const fl::u8 *perm) {
        return fl::s16x16::from_raw(
            pnoise2d_raw(fx.raw(), fy.raw(), fade_lut, perm));
    }

    // Raw i32 version: takes s16x16 raw values, returns s16x16 raw value.
    // Avoids from_raw/raw() round-trips when caller already has raw values.
    static inline fl::i32 pnoise2d_raw(fl::i32 fx_raw, fl::i32 fy_raw,
                                        const fl::i32 *fade_lut,
                                        const fl::u8 *perm) {
        int X, Y;
        fl::i32 x, y;
        floor_frac(fx_raw, X, x);
        floor_frac(fy_raw, Y, y);
        X &= 255;
        Y &= 255;

        fl::i32 u = fade(x, fade_lut);
        fl::i32 v = fade(y, fade_lut);

        int A  = perm[X & 255]       + Y;
        int AA = perm[A & 255];
        int AB = perm[(A + 1) & 255];
        int B  = perm[(X + 1) & 255] + Y;
        int BA = perm[B & 255];
        int BB = perm[(B + 1) & 255];

        fl::i32 result = lerp(v,
            lerp(u, grad(perm[AA & 255], x,          y),
                    grad(perm[BA & 255], x - HP_ONE, y)),
            lerp(u, grad(perm[AB & 255], x,          y - HP_ONE),
                    grad(perm[BB & 255], x - HP_ONE, y - HP_ONE)));

        return result >> (HP_BITS - fl::s16x16::FRAC_BITS);
    }

    static constexpr int FP_BITS = fl::s16x16::FRAC_BITS;
    static constexpr fl::i32 FP_ONE = 1 << FP_BITS;

    // Decompose s16x16 raw value into integer floor and Q8.24 fractional part.
    static FASTLED_FORCE_INLINE void floor_frac(fl::i32 fp16, int &ifloor,
                                                fl::i32 &frac24) {
        ifloor = fp16 >> FP_BITS;
        frac24 = (fp16 & (FP_ONE - 1)) << (HP_BITS - FP_BITS);
    }

    // LUT fade: 1 lookup + 1 lerp replaces 5 multiplies.
    static FASTLED_FORCE_INLINE fl::i32 fade(fl::i32 t, const fl::i32 *table) {
        fl::u32 idx = static_cast<fl::u32>(t) >> 16;
        fl::i32 frac = t & 0xFFFF;
        fl::i32 a = table[idx];
        fl::i32 b = table[idx + 1];
        return a + static_cast<fl::i32>(
            (static_cast<fl::i64>(frac) * (b - a)) >> 16);
    }

    static FASTLED_FORCE_INLINE fl::i32 lerp(fl::i32 t, fl::i32 a, fl::i32 b) {
        return a + static_cast<fl::i32>(
            (static_cast<fl::i64>(t) * (b - a)) >> HP_BITS);
    }

    // z=0 gradient via branchless coefficient LUT.
    static FASTLED_FORCE_INLINE fl::i32 grad(int hash, fl::i32 x, fl::i32 y) {
        struct GradCoeff { fl::i8 cx; fl::i8 cy; };
        constexpr GradCoeff lut[16] = {
            { 1,  1}, {-1,  1}, { 1, -1}, {-1, -1},
            { 1,  0}, {-1,  0}, { 1,  0}, {-1,  0},
            { 0,  1}, { 0, -1}, { 0,  1}, { 0, -1},
            { 1,  1}, { 0, -1}, {-1,  1}, { 0, -1},
        };
        const GradCoeff &g = lut[hash & 15];
        return g.cx * x + g.cy * y;
    }
};

} // namespace fx
} // namespace fl
