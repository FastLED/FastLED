#pragma once

// 2D Perlin noise SIMD implementation using s16x16 fixed-point arithmetic
// Extracted from animartrix2_detail.hpp for testing and debugging
//
// SIMD batch version: Process 4 Perlin evaluations in parallel
// Uses FastLED SIMD abstraction layer for vectorizable operations

#include "fl/fx/2d/detail/perlin_s16x16.hpp"
#include "fl/simd.h"

namespace fl {
namespace fx {

struct perlin_s16x16_simd {
    static constexpr int HP_BITS = perlin_s16x16::HP_BITS;
    static constexpr fl::i32 HP_ONE = perlin_s16x16::HP_ONE;
    static constexpr int FP_BITS = fl::s16x16::FRAC_BITS;
    static constexpr fl::i32 FP_ONE = 1 << FP_BITS;

    // SIMD batch version: Process 4 Perlin evaluations in parallel
    static inline void pnoise2d_raw_simd4(
        const fl::i32 nx[4], const fl::i32 ny[4],
        const fl::i32 *fade_lut, const fl::u8 *perm,
        fl::i32 out[4])
    {
        // SIMD: Load input coordinates as vectors
        fl::simd::simd_u32x4 nx_vec = fl::simd::load_u32_4(reinterpret_cast<const fl::u32*>(nx)); // ok reinterpret cast
        fl::simd::simd_u32x4 ny_vec = fl::simd::load_u32_4(reinterpret_cast<const fl::u32*>(ny)); // ok reinterpret cast

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

        // Extract to arrays for scalar operations (permutation lookups, fade LUT)
        fl::u32 X[4], Y[4];
        fl::i32 x_frac[4], y_frac[4];
        fl::simd::store_u32_4(X, X_vec);
        fl::simd::store_u32_4(Y, Y_vec);
        fl::simd::store_u32_4(reinterpret_cast<fl::u32*>(x_frac), x_frac_vec); // ok reinterpret cast
        fl::simd::store_u32_4(reinterpret_cast<fl::u32*>(y_frac), y_frac_vec); // ok reinterpret cast

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

        // Gradient computations (scalar - faster than SIMD for small computations)
        fl::i32 g_aa[4], g_ba[4], g_ab[4], g_bb[4];
        for (int i = 0; i < 4; i++) {
            g_aa[i] = perlin_s16x16::grad(perm[AA[i] & 255], x_frac[i],           y_frac[i]);
            g_ba[i] = perlin_s16x16::grad(perm[BA[i] & 255], x_frac[i] - HP_ONE, y_frac[i]);
            g_ab[i] = perlin_s16x16::grad(perm[AB[i] & 255], x_frac[i],           y_frac[i] - HP_ONE);
            g_bb[i] = perlin_s16x16::grad(perm[BB[i] & 255], x_frac[i] - HP_ONE, y_frac[i] - HP_ONE);
        }

        // SIMD: Vectorized lerp (3 levels of interpolation)
        // Load gradient vectors
        fl::simd::simd_u32x4 u_vec = fl::simd::load_u32_4(reinterpret_cast<const fl::u32*>(u)); // ok reinterpret cast
        fl::simd::simd_u32x4 v_vec = fl::simd::load_u32_4(reinterpret_cast<const fl::u32*>(v)); // ok reinterpret cast
        fl::simd::simd_u32x4 g_aa_vec = fl::simd::load_u32_4(reinterpret_cast<const fl::u32*>(g_aa)); // ok reinterpret cast
        fl::simd::simd_u32x4 g_ba_vec = fl::simd::load_u32_4(reinterpret_cast<const fl::u32*>(g_ba)); // ok reinterpret cast
        fl::simd::simd_u32x4 g_ab_vec = fl::simd::load_u32_4(reinterpret_cast<const fl::u32*>(g_ab)); // ok reinterpret cast
        fl::simd::simd_u32x4 g_bb_vec = fl::simd::load_u32_4(reinterpret_cast<const fl::u32*>(g_bb)); // ok reinterpret cast

        // SIMD: lerp1 = lerp(u, g_aa, g_ba) = g_aa + ((g_ba - g_aa) * u) >> HP_BITS
        // Fix: Pre-shift diff to avoid losing high bits in mulhi
        // Instead of: (diff * u) >> 16 >> 8, do: ((diff >> 8) * u) >> 16
        fl::simd::simd_u32x4 diff1 = fl::simd::sub_i32_4(g_ba_vec, g_aa_vec);
        diff1 = fl::simd::sra_i32_4(diff1, HP_BITS - 16);                      // Pre-shift: >> 8 (Q8.24 -> Q8.16)
        fl::simd::simd_u32x4 lerp1_vec = fl::simd::mulhi_i32_4(diff1, u_vec);  // (Q8.16 * Q8.24) >> 16 = Q8.24
        lerp1_vec = fl::simd::add_i32_4(g_aa_vec, lerp1_vec);

        // SIMD: lerp2 = lerp(u, g_ab, g_bb) = g_ab + ((g_bb - g_ab) * u) >> HP_BITS
        fl::simd::simd_u32x4 diff2 = fl::simd::sub_i32_4(g_bb_vec, g_ab_vec);
        diff2 = fl::simd::sra_i32_4(diff2, HP_BITS - 16);                      // Pre-shift: >> 8
        fl::simd::simd_u32x4 lerp2_vec = fl::simd::mulhi_i32_4(diff2, u_vec);  // (Q8.16 * Q8.24) >> 16 = Q8.24
        lerp2_vec = fl::simd::add_i32_4(g_ab_vec, lerp2_vec);

        // SIMD: final = lerp(v, lerp1, lerp2) = lerp1 + ((lerp2 - lerp1) * v) >> HP_BITS
        fl::simd::simd_u32x4 diff3 = fl::simd::sub_i32_4(lerp2_vec, lerp1_vec);
        diff3 = fl::simd::sra_i32_4(diff3, HP_BITS - 16);                      // Pre-shift: >> 8
        fl::simd::simd_u32x4 final_vec = fl::simd::mulhi_i32_4(diff3, v_vec);  // (Q8.16 * Q8.24) >> 16 = Q8.24
        final_vec = fl::simd::add_i32_4(lerp1_vec, final_vec);

        // SIMD: Shift to match s16x16 fractional bits (arithmetic for signed values)
        final_vec = fl::simd::sra_i32_4(final_vec, HP_BITS - fl::s16x16::FRAC_BITS);

        // Store result
        fl::simd::store_u32_4(reinterpret_cast<fl::u32*>(out), final_vec); // ok reinterpret cast
    }
};

} // namespace fx
} // namespace fl
