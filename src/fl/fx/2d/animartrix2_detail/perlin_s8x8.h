#pragma once

// 2D Perlin noise s8x8 implementation
// Extracted from animartrix2_detail.hpp
//
// s8x8 Perlin: Ultra-fast 8-bit variant for maximum speed with reduced precision
// Uses 8 fractional bits throughout. Trades accuracy for speed (4x faster multiplies vs i32).

#include "fl/fixed_point/s16x16.h"
#include "fl/force_inline.h"

namespace fl {

struct perlin_s8x8 {
    static constexpr int HP_BITS = 8;  // Q8 precision!
    static constexpr fl::i32 HP_ONE = 1 << HP_BITS; // 256 = 1.0

    // Build 257-entry Perlin fade LUT in Q8 format (8 fractional bits).
    static void init_fade_lut(fl::i32 *table);

    // 2D Perlin noise. Input s16x16, output s16x16 approx [-1, 1].
    static fl::s16x16 pnoise2d(fl::s16x16 fx, fl::s16x16 fy,
                                      const fl::i32 *fade_lut,
                                      const fl::u8 *perm);

    // Raw i32 version using Q8 internal precision.
    // Fast path: all arithmetic uses i16 operations (except final shift).
    static fl::i32 pnoise2d_raw(fl::i32 fx_raw, fl::i32 fy_raw,
                                        const fl::i32 *fade_lut,
                                        const fl::u8 *perm);

  private:
    static constexpr int FP_BITS = fl::s16x16::FRAC_BITS;
    static constexpr fl::i32 FP_ONE = 1 << FP_BITS;

    // Decompose s16x16 raw value into integer floor and Q8 fractional part.
    static FASTLED_FORCE_INLINE void floor_frac(fl::i32 fp16, int &ifloor,
                                                fl::i16 &frac8);

    // LUT fade: Direct table lookup (t is already 8-bit index)
    static FASTLED_FORCE_INLINE fl::i16 fade(fl::i16 t, const fl::i32 *table);

    static FASTLED_FORCE_INLINE fl::i16 lerp(fl::i16 t, fl::i16 a, fl::i16 b);

    // z=0 gradient via branchless coefficient LUT (Q8 format).
    static FASTLED_FORCE_INLINE fl::i16 grad(int hash, fl::i16 x, fl::i16 y);
};

}  // namespace fl
