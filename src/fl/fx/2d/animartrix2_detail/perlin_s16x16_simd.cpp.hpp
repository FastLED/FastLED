#pragma once
// allow-include-after-namespace

// 2D Perlin noise SIMD implementation using s16x16 fixed-point arithmetic
// Implementation file - included from perlin_s16x16_simd.h

#include "fl/compiler_control.h"
#include "fl/fx/2d/animartrix2_detail/perlin_s16x16_simd.h"
#include "fl/simd.h"
#include "fl/fx/2d/animartrix2_detail/perlin_s16x16.h"

FL_OPTIMIZATION_LEVEL_O3_BEGIN

namespace fl {

// Primary overload: accepts SIMD registers directly.
// Performs SIMD floor/frac/wrap, then exits to scalar for fade/perm/grad/lerp
// (SSE2 has no integer gather), and re-packs the result into a SIMD register.
fl::simd::simd_u32x4 perlin_s16x16_simd::pnoise2d_raw_simd4_vec(
    fl::simd::simd_u32x4 nx_vec, fl::simd::simd_u32x4 ny_vec,
    const fl::i32 *fade_lut, const fl::u8 *perm)
{
    // SIMD: Extract integer floor (shift right by FP_BITS)
    fl::simd::simd_u32x4 X_vec = fl::simd::srl_u32_4(nx_vec, FP_BITS);
    fl::simd::simd_u32x4 Y_vec = fl::simd::srl_u32_4(ny_vec, FP_BITS);

    // SIMD: Extract fractional part and shift to HP_BITS
    // Convert from Q16.16 to Q8.24 by shifting left 8 bits
    fl::simd::simd_u32x4 mask_fp = fl::simd::set1_u32_4(FP_ONE - 1);
    fl::simd::simd_u32x4 x_frac_vec = fl::simd::and_u32_4(nx_vec, mask_fp);
    fl::simd::simd_u32x4 y_frac_vec = fl::simd::and_u32_4(ny_vec, mask_fp);

    // Use the new sll_u32_4 operation for left shift
    x_frac_vec = fl::simd::sll_u32_4(x_frac_vec, 8);
    y_frac_vec = fl::simd::sll_u32_4(y_frac_vec, 8);

    // SIMD: Wrap to [0, 255]
    fl::simd::simd_u32x4 mask_255 = fl::simd::set1_u32_4(255);
    X_vec = fl::simd::and_u32_4(X_vec, mask_255);
    Y_vec = fl::simd::and_u32_4(Y_vec, mask_255);
    // ── [end SIMD coordinate arithmetic] ──────────────────────────────────────

    // ── [BOUNDARY D+E: SIMD extract → per-lane scalar → SIMD re-pack] ───────
    // Extract coordinates directly from SIMD registers (no intermediate arrays).
    // Each lane does: fade LUT, perm table, grad, lerp — fully self-contained.
    // SSE2 has no integer gather, so scalar random-access is unavoidable.
    constexpr int SHIFT = HP_BITS - fl::s16x16::FRAC_BITS;
    auto lane = [&](int i) -> fl::i32 {
        fl::u32 Xi = fl::simd::extract_u32_4(X_vec, i);
        fl::u32 Yi = fl::simd::extract_u32_4(Y_vec, i);
        fl::i32 xf = static_cast<fl::i32>(fl::simd::extract_u32_4(x_frac_vec, i));
        fl::i32 yf = static_cast<fl::i32>(fl::simd::extract_u32_4(y_frac_vec, i));

        fl::i32 u = perlin_s16x16::fade(xf, fade_lut);
        fl::i32 v = perlin_s16x16::fade(yf, fade_lut);

        int A  = perm[Xi & 255]       + Yi;
        int AA = perm[A & 255];
        int AB = perm[(A + 1) & 255];
        int B  = perm[(Xi + 1) & 255] + Yi;
        int BA = perm[B & 255];
        int BB = perm[(B + 1) & 255];

        fl::i32 g_aa = perlin_s16x16::grad(perm[AA & 255], xf,          yf);
        fl::i32 g_ba = perlin_s16x16::grad(perm[BA & 255], xf - HP_ONE, yf);
        fl::i32 g_ab = perlin_s16x16::grad(perm[AB & 255], xf,          yf - HP_ONE);
        fl::i32 g_bb = perlin_s16x16::grad(perm[BB & 255], xf - HP_ONE, yf - HP_ONE);
        fl::i32 lerp1 = perlin_s16x16::lerp(u, g_aa, g_ba);
        fl::i32 lerp2 = perlin_s16x16::lerp(u, g_ab, g_bb);
        return perlin_s16x16::lerp(v, lerp1, lerp2) >> SHIFT;
    };
    return fl::simd::set_u32_4(
        static_cast<fl::u32>(lane(0)), static_cast<fl::u32>(lane(1)),
        static_cast<fl::u32>(lane(2)), static_cast<fl::u32>(lane(3)));
    // ── [end BOUNDARY D+E] ───────────────────────────────────────────────────
}


}  // namespace fl

FL_OPTIMIZATION_LEVEL_O3_END
