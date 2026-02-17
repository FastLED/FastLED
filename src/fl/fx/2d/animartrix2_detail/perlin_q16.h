#pragma once

// 2D Perlin noise Q16 implementation
// Extracted from animartrix2_detail.hpp
//
// Q16 variant: Uses 16 fractional bits instead of 24 for faster arithmetic.
// Trades internal precision for speed: i32 ops instead of i64, smaller LUT.

#include "fl/fixed_point/s16x16.h"
#include "fl/force_inline.h"

namespace fl {

struct perlin_q16 {
    static constexpr int HP_BITS = 16;
    static constexpr fl::i32 HP_ONE = 1 << HP_BITS; // 65536 = 1.0

    // Build 257-entry Perlin fade LUT in Q16 format (16 fractional bits).
    static void init_fade_lut(fl::i32 *table);

    // 2D Perlin noise. Input s16x16, output s16x16 approx [-1, 1].
    static fl::s16x16 pnoise2d(fl::s16x16 fx, fl::s16x16 fy,
                                      const fl::i32 *fade_lut,
                                      const fl::u8 *perm);

    // Raw i32 version using Q16 internal precision.
    static fl::i32 pnoise2d_raw(fl::i32 fx_raw, fl::i32 fy_raw,
                                        const fl::i32 *fade_lut,
                                        const fl::u8 *perm);

  private:
    static constexpr int FP_BITS = fl::s16x16::FRAC_BITS;
    static constexpr fl::i32 FP_ONE = 1 << FP_BITS;

    // Decompose s16x16 raw value into integer floor and Q16 fractional part.
    static FASTLED_FORCE_INLINE void floor_frac(fl::i32 fp16, int &ifloor,
                                                fl::i32 &frac16);

    // LUT fade: 1 lookup + 1 lerp replaces 5 multiplies.
    static FASTLED_FORCE_INLINE fl::i32 fade(fl::i32 t, const fl::i32 *table);

    static FASTLED_FORCE_INLINE fl::i32 lerp(fl::i32 t, fl::i32 a, fl::i32 b);

    // z=0 gradient via branchless coefficient LUT (Q16 format).
    static FASTLED_FORCE_INLINE fl::i32 grad(int hash, fl::i32 x, fl::i32 y);
};

}  // namespace fl
