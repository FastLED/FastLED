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

    // SIMD batch version: Process 4 Perlin evaluations in parallel, write to out[4]
    static void pnoise2d_raw_simd4(
        const fl::i32 nx[4], const fl::i32 ny[4],
        const fl::i32 *fade_lut, const fl::u8 *perm,
        fl::i32 out[4]);

    // Same as pnoise2d_raw_simd4 but returns result as simd_u32x4 register.
    // Avoids a store-then-reload at the call site when the result feeds SIMD ops.
    static fl::simd::simd_u32x4 pnoise2d_raw_simd4_vec(
        const fl::i32 nx[4], const fl::i32 ny[4],
        const fl::i32 *fade_lut, const fl::u8 *perm);

    // Register-accepting overload: takes SIMD registers directly, eliminating
    // the store→reload round-trip (boundaries B+C) when the caller already has
    // coordinates in SIMD registers.
    static fl::simd::simd_u32x4 pnoise2d_raw_simd4_vec(
        fl::simd::simd_u32x4 nx_vec, fl::simd::simd_u32x4 ny_vec,
        const fl::i32 *fade_lut, const fl::u8 *perm);
};

}  // namespace fl
