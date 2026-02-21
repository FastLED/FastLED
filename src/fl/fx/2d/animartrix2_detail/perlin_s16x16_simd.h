#pragma once

// 2D Perlin noise SIMD implementation using s16x16 fixed-point arithmetic
// Extracted from animartrix2_detail.hpp for testing and debugging
//
// SIMD batch version: Process 4 Perlin evaluations in parallel
// Uses FastLED SIMD abstraction layer for vectorizable operations

#include "fl/fx/2d/animartrix2_detail/perlin_s16x16.h"
#include "fl/simd.h"

namespace fl {


struct perlin_s16x16_simd {
    static constexpr int HP_BITS = perlin_s16x16::HP_BITS;
    static constexpr fl::i32 HP_ONE = perlin_s16x16::HP_ONE;
    static constexpr int FP_BITS = fl::s16x16::FRAC_BITS;
    static constexpr fl::i32 FP_ONE = 1 << FP_BITS;

    // SIMD batch version: Process 4 Perlin evaluations in parallel.
    // Takes SIMD registers directly — the caller loads/stores as needed.
    static fl::simd::simd_u32x4 pnoise2d_raw_simd4_vec(
        fl::simd::simd_u32x4 nx_vec, fl::simd::simd_u32x4 ny_vec,
        const fl::i32 *fade_lut, const fl::u8 *perm);
};

}  // namespace fl
