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

    // ── [BOUNDARY D: SIMD unpack → scalar arrays for gather] ─────────────────
    // The processed coordinates (integer floor X/Y, fractional x_frac/y_frac)
    // are needed as scalar indices into perm[] and fade_lut[] via random-access
    // lookups.  SSE2 has no integer gather instruction (that requires AVX2), so
    // we must exit SIMD here. All 4 sets of coordinates are stored to stack
    // arrays and the rest of the Perlin evaluation runs as scalar loops.
    // This is the primary scalar bottleneck in the SIMD path.
    fl::u32 X[4], Y[4];
    fl::i32 x_frac[4], y_frac[4];
    fl::simd::store_u32_4(X, X_vec);
    fl::simd::store_u32_4(Y, Y_vec);
    fl::simd::store_u32_4(reinterpret_cast<fl::u32*>(x_frac), x_frac_vec); // ok reinterpret cast
    fl::simd::store_u32_4(reinterpret_cast<fl::u32*>(y_frac), y_frac_vec); // ok reinterpret cast
    // ── [end BOUNDARY D] ──────────────────────────────────────────────────────

    // SCALAR: Fade LUT lookups (requires gather, not available on SSE2)
    fl::i32 u[4], v[4];
    for (int i = 0; i < 4; i++) {
        u[i] = perlin_s16x16::fade(x_frac[i], fade_lut);
        v[i] = perlin_s16x16::fade(y_frac[i], fade_lut);
    }

    // Permutation lookups (scalar - faster than AVX2 gather for small SIMD width)
    int A[4], AA[4], AB[4], B[4], BA[4], BB[4];
    for (int i = 0; i < 4; i++) {
        A[i]  = perm[X[i] & 255]       + Y[i];
        AA[i] = perm[A[i] & 255];
        AB[i] = perm[(A[i] + 1) & 255];
        B[i]  = perm[(X[i] + 1) & 255] + Y[i];
        BA[i] = perm[B[i] & 255];
        BB[i] = perm[(B[i] + 1) & 255];
    }

    // Gradient computations and interpolation (scalar to match scalar implementation exactly)
    // The SIMD lerp with pre-shift loses precision due to truncation at different points
    // Use scalar lerp to guarantee exact match with scalar implementation
    // ── [BOUNDARY E: scalar results → SIMD re-pack via set_u32_4] ────────────
    constexpr int SHIFT = HP_BITS - fl::s16x16::FRAC_BITS;
    auto lane = [&](int i) -> fl::i32 {
        fl::i32 g_aa = perlin_s16x16::grad(perm[AA[i] & 255], x_frac[i],           y_frac[i]);
        fl::i32 g_ba = perlin_s16x16::grad(perm[BA[i] & 255], x_frac[i] - HP_ONE, y_frac[i]);
        fl::i32 g_ab = perlin_s16x16::grad(perm[AB[i] & 255], x_frac[i],           y_frac[i] - HP_ONE);
        fl::i32 g_bb = perlin_s16x16::grad(perm[BB[i] & 255], x_frac[i] - HP_ONE, y_frac[i] - HP_ONE);
        fl::i32 lerp1 = perlin_s16x16::lerp(u[i], g_aa, g_ba);
        fl::i32 lerp2 = perlin_s16x16::lerp(u[i], g_ab, g_bb);
        return perlin_s16x16::lerp(v[i], lerp1, lerp2) >> SHIFT;
    };
    return fl::simd::set_u32_4(
        static_cast<fl::u32>(lane(0)), static_cast<fl::u32>(lane(1)),
        static_cast<fl::u32>(lane(2)), static_cast<fl::u32>(lane(3)));
    // ── [end BOUNDARY E] ──────────────────────────────────────────────────────
}


}  // namespace fl

FL_OPTIMIZATION_LEVEL_O3_END
