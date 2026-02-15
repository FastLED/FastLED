#pragma once
// allow-include-after-namespace

// Animartrix2 detail: Refactored version of animartrix_detail.hpp
// Original by Stefan Petrick 2023. Adapted to C++ by Netmindz 2023.
// Adapted to FastLED by Zach Vorhies 2024.
// Refactored to free-function architecture 2026.
//
// Licensed under Creative Commons Attribution License CC BY-NC 3.0
// https://creativecommons.org/licenses/by-nc/3.0/
//
// Architecture: Context struct holds all shared state. Each animation is a
// free function (Visualizer) that operates on Context. Internally delegates
// to animartrix_detail::ANIMartRIX for bit-identical output.

#ifndef ANIMARTRIX2_INTERNAL
#error "This file is not meant to be included directly. Include animartrix2.hpp instead."
#endif

// Include the original detail for its ANIMartRIX base class
#define ANIMARTRIX_INTERNAL
#include "fl/fx/2d/animartrix_detail.hpp"

#include "crgb.h"
#include "fl/stl/vector.h"
#include "fl/stl/math.h"
#include "fl/stl/optional.h"
#include "fl/stl/stdint.h"
#include "fl/sin32.h"
#include "fl/fixed_point/s16x16.h"


#ifndef PI
#define PI 3.1415926535897932384626433832795
#endif

#ifndef FL_ANIMARTRIX_USES_FAST_MATH
#define FL_ANIMARTRIX_USES_FAST_MATH 1
#endif

#if FL_ANIMARTRIX_USES_FAST_MATH
FL_FAST_MATH_BEGIN
FL_OPTIMIZATION_LEVEL_O3_BEGIN
#endif

namespace animartrix2_detail {

// Forward declaration
struct Context;

// Visualizer: a free function that operates on Context to render one frame
using Visualizer = void (*)(Context &ctx);

// Callback for mapping (x,y) to a 1D LED index
using XYMapCallback = fl::u16 (*)(fl::u16 x, fl::u16 y, void *userData);

// Context: All shared state for animations, passed to free-function visualizers.
// Internally wraps an animartrix_detail::ANIMartRIX to reuse existing logic.
struct Context {
    // Grid dimensions
    int num_x = 0;
    int num_y = 0;

    // Output target
    CRGB *leds = nullptr;
    XYMapCallback xyMapFn = nullptr;
    void *xyMapUserData = nullptr;

    // Time
    fl::optional<fl::u32> currentTime;

    // Internal engine (reuses original implementation for bit-identical output)
    struct Engine;
    Engine *mEngine = nullptr;

    Context() = default;
    ~Context();
    Context(const Context &) = delete;
    Context &operator=(const Context &) = delete;
};

// Per-pixel pre-computed s16x16 values for Chasing_Spirals inner loop.
// These are constant per-frame (depend only on grid geometry, not time).
// Defined here (above Engine) so Engine can hold a persistent vector of them.
struct ChasingSpiralPixelLUT {
    fl::s16x16 base_angle;     // 3*theta - dist/3
    fl::s16x16 dist_scaled;    // distance * scale (0.1), pre-scaled for noise coords
    fl::s16x16 rf3;            // 3 * radial_filter (for red channel)
    fl::s16x16 rf_half;        // radial_filter / 2 (for green channel)
    fl::s16x16 rf_quarter;     // radial_filter / 4 (for blue channel)
    fl::u16 pixel_idx;        // Pre-computed xyMap(x, y) output pixel index
};

// LUT-accelerated 2D Perlin noise using s16x16 fixed-point.
// Internals use Q8.24 (24 fractional bits) for precision exceeding float.
// The fade LUT replaces the 6t^5-15t^4+10t^3 polynomial with table lookup.
// The z=0 specialization halves work vs full 3D noise.
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

    // SIMD batch version: Process 4 Perlin evaluations in parallel
    // Uses true SIMD via FastLED abstraction layer for vectorizable operations
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
        fl::simd::simd_u32x4 mask_fp = fl::simd::set1_u32_4(FP_ONE - 1);
        fl::simd::simd_u32x4 x_frac_vec = fl::simd::and_u32_4(nx_vec, mask_fp);
        fl::simd::simd_u32x4 y_frac_vec = fl::simd::and_u32_4(ny_vec, mask_fp);
        x_frac_vec = fl::simd::srl_u32_4(x_frac_vec, FP_BITS - HP_BITS);
        y_frac_vec = fl::simd::srl_u32_4(y_frac_vec, FP_BITS - HP_BITS);

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
            u[i] = fade(x_frac[i], fade_lut);
            v[i] = fade(y_frac[i], fade_lut);
        }

        // Permutation lookups (scalar - faster than AVX2 gather for small SIMD width)
        // Note: AVX2 gather tested but 3-7% SLOWER due to high latency (see perlin_avx2_gather_results.md)
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
        // Note: Vectorized version tested but 7% SLOWER due to setup overhead
        fl::i32 g_aa[4], g_ba[4], g_ab[4], g_bb[4];
        for (int i = 0; i < 4; i++) {
            g_aa[i] = grad(perm[AA[i] & 255], x_frac[i],           y_frac[i]);
            g_ba[i] = grad(perm[BA[i] & 255], x_frac[i] - HP_ONE, y_frac[i]);
            g_ab[i] = grad(perm[AB[i] & 255], x_frac[i],           y_frac[i] - HP_ONE);
            g_bb[i] = grad(perm[BB[i] & 255], x_frac[i] - HP_ONE, y_frac[i] - HP_ONE);
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
        fl::simd::simd_u32x4 diff1 = fl::simd::sub_i32_4(g_ba_vec, g_aa_vec);
        fl::simd::simd_u32x4 lerp1_vec = fl::simd::mulhi_i32_4(diff1, u_vec);  // High 16 bits of 32×32→64 multiply
        lerp1_vec = fl::simd::add_i32_4(g_aa_vec, lerp1_vec);

        // SIMD: lerp2 = lerp(u, g_ab, g_bb) = g_ab + ((g_bb - g_ab) * u) >> HP_BITS
        fl::simd::simd_u32x4 diff2 = fl::simd::sub_i32_4(g_bb_vec, g_ab_vec);
        fl::simd::simd_u32x4 lerp2_vec = fl::simd::mulhi_i32_4(diff2, u_vec);
        lerp2_vec = fl::simd::add_i32_4(g_ab_vec, lerp2_vec);

        // SIMD: final = lerp(v, lerp1, lerp2) = lerp1 + ((lerp2 - lerp1) * v) >> HP_BITS
        fl::simd::simd_u32x4 diff3 = fl::simd::sub_i32_4(lerp2_vec, lerp1_vec);
        fl::simd::simd_u32x4 final_vec = fl::simd::mulhi_i32_4(diff3, v_vec);
        final_vec = fl::simd::add_i32_4(lerp1_vec, final_vec);

        // SIMD: Shift to match s16x16 fractional bits
        final_vec = fl::simd::srl_u32_4(final_vec, HP_BITS - fl::s16x16::FRAC_BITS);

        // Store result
        fl::simd::store_u32_4(reinterpret_cast<fl::u32*>(out), final_vec); // ok reinterpret cast
    }

  private:
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

// Q16 variant: Uses 16 fractional bits instead of 24 for faster arithmetic.
// Trades internal precision for speed: i32 ops instead of i64, smaller LUT.
struct perlin_q16 {
    static constexpr int HP_BITS = 16;
    static constexpr fl::i32 HP_ONE = 1 << HP_BITS; // 65536 = 1.0

    // Build 257-entry Perlin fade LUT in Q16 format (16 fractional bits).
    static inline void init_fade_lut(fl::i32 *table) {
        for (int i = 0; i <= 256; i++) {
            fl::i32 t = (i * HP_ONE) / 256;
            fl::i32 t2 = static_cast<fl::i32>((static_cast<fl::i64>(t) * t) >> HP_BITS);
            fl::i32 t3 = static_cast<fl::i32>((static_cast<fl::i64>(t2) * t) >> HP_BITS);
            fl::i32 inner = static_cast<fl::i32>((static_cast<fl::i64>(t) * (6 * HP_ONE)) >> HP_BITS);
            inner -= 15 * HP_ONE;
            inner = static_cast<fl::i32>((static_cast<fl::i64>(t) * inner) >> HP_BITS);
            inner += 10 * HP_ONE;
            table[i] = static_cast<fl::i32>((static_cast<fl::i64>(t3) * inner) >> HP_BITS);
        }
    }

    // 2D Perlin noise. Input s16x16, output s16x16 approx [-1, 1].
    static inline fl::s16x16 pnoise2d(fl::s16x16 fx, fl::s16x16 fy,
                                      const fl::i32 *fade_lut,
                                      const fl::u8 *perm) {
        return fl::s16x16::from_raw(
            pnoise2d_raw(fx.raw(), fy.raw(), fade_lut, perm));
    }

    // Raw i32 version using Q16 internal precision.
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

        // No shift needed: already in Q16 format, matches s16x16::FRAC_BITS
        return result;
    }

  private:
    static constexpr int FP_BITS = fl::s16x16::FRAC_BITS;
    static constexpr fl::i32 FP_ONE = 1 << FP_BITS;

    // Decompose s16x16 raw value into integer floor and Q16 fractional part.
    static FASTLED_FORCE_INLINE void floor_frac(fl::i32 fp16, int &ifloor,
                                                fl::i32 &frac16) {
        ifloor = fp16 >> FP_BITS;
        frac16 = fp16 & (FP_ONE - 1); // Already Q16, no shift needed
    }

    // LUT fade: 1 lookup + 1 lerp replaces 5 multiplies.
    static FASTLED_FORCE_INLINE fl::i32 fade(fl::i32 t, const fl::i32 *table) {
        fl::u32 idx = static_cast<fl::u32>(t) >> 8; // Q16 → 8-bit index
        fl::i32 frac = t & 0xFF;
        fl::i32 a = table[idx];
        fl::i32 b = table[idx + 1];
        // Lerp in Q16: frac is 8 bits, expand to 16 for precision
        return a + static_cast<fl::i32>(
            (static_cast<fl::i32>(frac << 8) * (b - a)) >> 16);
    }

    static FASTLED_FORCE_INLINE fl::i32 lerp(fl::i32 t, fl::i32 a, fl::i32 b) {
        // All values in Q16, result stays Q16
        return a + static_cast<fl::i32>(
            (static_cast<fl::i64>(t) * (b - a)) >> HP_BITS);
    }

    // z=0 gradient via branchless coefficient LUT (Q16 format).
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

// s8x8 Perlin: Ultra-fast 8-bit variant for maximum speed with reduced precision
// Uses 8 fractional bits throughout. Trades accuracy for speed (4x faster multiplies vs i32).
struct perlin_s8x8 {
    static constexpr int HP_BITS = 8;  // Q8 precision!
    static constexpr fl::i32 HP_ONE = 1 << HP_BITS; // 256 = 1.0

    // Build 257-entry Perlin fade LUT in Q8 format (8 fractional bits).
    static inline void init_fade_lut(fl::i32 *table) {
        for (int i = 0; i <= 256; i++) {
            fl::i16 t = static_cast<fl::i16>((i * HP_ONE) / 256);
            fl::i16 t2 = static_cast<fl::i16>((static_cast<fl::i32>(t) * t) >> HP_BITS);
            fl::i16 t3 = static_cast<fl::i16>((static_cast<fl::i32>(t2) * t) >> HP_BITS);
            fl::i16 inner = static_cast<fl::i16>((static_cast<fl::i32>(t) * (6 * HP_ONE)) >> HP_BITS);
            inner -= 15 * HP_ONE;
            inner = static_cast<fl::i16>((static_cast<fl::i32>(t) * inner) >> HP_BITS);
            inner += 10 * HP_ONE;
            table[i] = static_cast<fl::i32>((static_cast<fl::i32>(t3) * inner) >> HP_BITS);
        }
    }

    // 2D Perlin noise. Input s16x16, output s16x16 approx [-1, 1].
    static inline fl::s16x16 pnoise2d(fl::s16x16 fx, fl::s16x16 fy,
                                      const fl::i32 *fade_lut,
                                      const fl::u8 *perm) {
        return fl::s16x16::from_raw(
            pnoise2d_raw(fx.raw(), fy.raw(), fade_lut, perm));
    }

    // Raw i32 version using Q8 internal precision.
    // Fast path: all arithmetic uses i16 operations (except final shift).
    static inline fl::i32 pnoise2d_raw(fl::i32 fx_raw, fl::i32 fy_raw,
                                        const fl::i32 *fade_lut,
                                        const fl::u8 *perm) {
        int X, Y;
        fl::i16 x, y;
        floor_frac(fx_raw, X, x);
        floor_frac(fy_raw, Y, y);
        X &= 255;
        Y &= 255;

        fl::i16 u = fade(x, fade_lut);
        fl::i16 v = fade(y, fade_lut);

        int A  = perm[X & 255]       + Y;
        int AA = perm[A & 255];
        int AB = perm[(A + 1) & 255];
        int B  = perm[(X + 1) & 255] + Y;
        int BA = perm[B & 255];
        int BB = perm[(B + 1) & 255];

        fl::i16 result = lerp(v,
            lerp(u, grad(perm[AA & 255], x,          y),
                    grad(perm[BA & 255], x - HP_ONE, y)),
            lerp(u, grad(perm[AB & 255], x,          y - HP_ONE),
                    grad(perm[BB & 255], x - HP_ONE, y - HP_ONE)));

        // Shift from Q8 to s16x16's Q16 format
        return static_cast<fl::i32>(result) << (fl::s16x16::FRAC_BITS - HP_BITS);
    }

  private:
    static constexpr int FP_BITS = fl::s16x16::FRAC_BITS;
    static constexpr fl::i32 FP_ONE = 1 << FP_BITS;

    // Decompose s16x16 raw value into integer floor and Q8 fractional part.
    static FASTLED_FORCE_INLINE void floor_frac(fl::i32 fp16, int &ifloor,
                                                fl::i16 &frac8) {
        ifloor = fp16 >> FP_BITS;
        fl::i32 frac16 = fp16 & (FP_ONE - 1);
        // Shift from 16 frac bits to 8 frac bits
        frac8 = static_cast<fl::i16>(frac16 >> (FP_BITS - HP_BITS));
    }

    // LUT fade: Direct table lookup (t is already 8-bit index)
    static FASTLED_FORCE_INLINE fl::i16 fade(fl::i16 t, const fl::i32 *table) {
        // t is Q8 (0-255 range), use directly as index
        fl::u8 idx = static_cast<fl::u8>(t);
        // Return LUT value, convert from i32 to i16 (values fit in Q8 range)
        return static_cast<fl::i16>(table[idx]);
    }

    static FASTLED_FORCE_INLINE fl::i16 lerp(fl::i16 t, fl::i16 a, fl::i16 b) {
        // All values in Q8, result stays Q8
        return static_cast<fl::i16>(
            a + (((static_cast<fl::i32>(t) * (b - a)) >> HP_BITS)));
    }

    // z=0 gradient via branchless coefficient LUT (Q8 format).
    static FASTLED_FORCE_INLINE fl::i16 grad(int hash, fl::i16 x, fl::i16 y) {
        struct GradCoeff { fl::i8 cx; fl::i8 cy; };
        constexpr GradCoeff lut[16] = {
            { 1,  1}, {-1,  1}, { 1, -1}, {-1, -1},
            { 1,  0}, {-1,  0}, { 1,  0}, {-1,  0},
            { 0,  1}, { 0, -1}, { 0,  1}, { 0, -1},
            { 1,  1}, { 0, -1}, {-1,  1}, { 0, -1},
        };
        const GradCoeff &g = lut[hash & 15];
        return static_cast<fl::i16>(g.cx * x + g.cy * y);
    }
};

// i16-optimized Perlin: Uses i16 for lerp/grad hot path (2x faster multiplies)
// Coordinates stay as i32 (s16x16) externally but convert to i16 for interpolation
struct perlin_i16_optimized {
    static constexpr int HP_BITS = 16;
    static constexpr fl::i32 HP_ONE = 1 << HP_BITS;

    // Build fade LUT - still i32 for API compatibility
    static inline void init_fade_lut(fl::i32 *table) {
        for (int i = 0; i <= 256; i++) {
            fl::i32 t = (i * HP_ONE) / 256;
            fl::i32 t2 = static_cast<fl::i32>((static_cast<fl::i64>(t) * t) >> HP_BITS);
            fl::i32 t3 = static_cast<fl::i32>((static_cast<fl::i64>(t2) * t) >> HP_BITS);
            fl::i32 inner = static_cast<fl::i32>((static_cast<fl::i64>(t) * (6 * HP_ONE)) >> HP_BITS);
            inner -= 15 * HP_ONE;
            inner = static_cast<fl::i32>((static_cast<fl::i64>(t) * inner) >> HP_BITS);
            inner += 10 * HP_ONE;
            table[i] = static_cast<fl::i32>((static_cast<fl::i64>(t3) * inner) >> HP_BITS);
        }
    }

    // Public API: accepts s16x16 raw values
    static inline fl::s16x16 pnoise2d(fl::s16x16 fx, fl::s16x16 fy,
                                      const fl::i32 *fade_lut,
                                      const fl::u8 *perm) {
        return fl::s16x16::from_raw(
            pnoise2d_raw(fx.raw(), fy.raw(), fade_lut, perm));
    }

    // Hot path: uses i16 arithmetic for lerp/grad after extracting fractional part
    static inline fl::i32 pnoise2d_raw(fl::i32 fx_raw, fl::i32 fy_raw,
                                        const fl::i32 *fade_lut,
                                        const fl::u8 *perm) {
        int X, Y;
        fl::i16 x16, y16;  // i16 fractional parts!
        floor_frac_i16(fx_raw, X, x16);
        floor_frac_i16(fy_raw, Y, y16);
        X &= 255;
        Y &= 255;

        // Fade values can be up to 65536, need to stay i32
        // But we can still optimize grad and internal lerp to use i16
        fl::i32 u = fade(x16, fade_lut);
        fl::i32 v = fade(y16, fade_lut);

        int A  = perm[X & 255]       + Y;
        int AA = perm[A & 255];
        int AB = perm[(A + 1) & 255];
        int B  = perm[(X + 1) & 255] + Y;
        int BA = perm[B & 255];
        int BB = perm[(B + 1) & 255];

        // grad returns i16 (coordinates are i16), lerp takes i32 fade values
        fl::i32 g00 = grad_i16(perm[AA & 255], x16,              y16);
        fl::i32 g10 = grad_i16(perm[BA & 255], x16 - HP_ONE_I16, y16);
        fl::i32 g01 = grad_i16(perm[AB & 255], x16,              y16 - HP_ONE_I16);
        fl::i32 g11 = grad_i16(perm[BB & 255], x16 - HP_ONE_I16, y16 - HP_ONE_I16);

        // Lerp with i32 (grad results fit in i16 range but lerp needs i32 for fade values)
        fl::i32 lerp0 = lerp(u, g00, g10);
        fl::i32 lerp1 = lerp(u, g01, g11);
        fl::i32 result = lerp(v, lerp0, lerp1);

        return result;
    }

  private:
    static constexpr int FP_BITS = fl::s16x16::FRAC_BITS;
    static constexpr fl::i32 FP_ONE = 1 << FP_BITS;
    // For i16 coordinates: use full 16-bit value (no room for 1.0 in i16)
    // Coordinates are fractional parts only (0-65535), HP_ONE stays as i32
    static constexpr fl::i32 HP_ONE_I16 = HP_ONE;

    // Extract fractional part as i16 (range 0-65535)
    static FASTLED_FORCE_INLINE void floor_frac_i16(fl::i32 fp16, int &ifloor, fl::i16 &frac16) {
        ifloor = fp16 >> FP_BITS;
        frac16 = static_cast<fl::i16>(fp16 & (FP_ONE - 1));  // Cast to i16
    }

    // Fade optimized for i16 input (coordinates), returns i32 (can be 0-65536)
    static FASTLED_FORCE_INLINE fl::i32 fade(fl::i16 t, const fl::i32 *table) {
        fl::u32 idx = static_cast<fl::u32>(t) >> 8;  // i16 → 8-bit index
        fl::i32 frac = t & 0xFF;
        fl::i32 a = table[idx];
        fl::i32 b = table[idx + 1];
        return a + ((static_cast<fl::i32>(frac << 8) * (b - a)) >> 16);
    }

    // Standard lerp (i32), but benefits from smaller grad results
    static FASTLED_FORCE_INLINE fl::i32 lerp(fl::i32 t, fl::i32 a, fl::i32 b) {
        return a + static_cast<fl::i32>(
            (static_cast<fl::i64>(t) * (b - a)) >> HP_BITS);
    }

    // i16 grad: Takes i16 coordinates (faster than i32), returns i32
    static FASTLED_FORCE_INLINE fl::i32 grad_i16(int hash, fl::i16 x, fl::i16 y) {
        struct GradCoeff { fl::i8 cx; fl::i8 cy; };
        constexpr GradCoeff lut[16] = {
            { 1,  1}, {-1,  1}, { 1, -1}, {-1, -1},
            { 1,  0}, {-1,  0}, { 1,  0}, {-1,  0},
            { 0,  1}, { 0, -1}, { 0,  1}, { 0, -1},
            { 1,  1}, { 0, -1}, {-1,  1}, { 0, -1},
        };
        const GradCoeff &g = lut[hash & 15];
        // i8 × i16 → i32 (safe, result in range ±65535)
        return static_cast<fl::i32>(g.cx * x) + static_cast<fl::i32>(g.cy * y);
    }
};

// Bridge class: connects ANIMartRIX to Context's output callbacks
struct Context::Engine : public animartrix_detail::ANIMartRIX {
    Context *mCtx;

    // Persistent PixelLUT for Chasing_Spirals_Q31.
    // Depends only on grid geometry (polar_theta, distance, radial_filter_radius),
    // which is constant across frames. Computed once on first use, reused every frame.
    fl::vector<ChasingSpiralPixelLUT> mChasingSpiralLUT;

    // Persistent hp_fade LUT for Perlin noise (257 entries, Q8.24 format).
    // Replaces 5 multiplies per hp_fade call with table lookup + lerp.
    // Initialized on first use by q31::Chasing_Spirals_Q31.
    fl::i32 mFadeLUT[257];
    bool mFadeLUTInitialized;

    Engine(Context *ctx) : mCtx(ctx), mFadeLUT{}, mFadeLUTInitialized(false) {}

    void setPixelColorInternal(int x, int y,
                               animartrix_detail::rgb pixel) override {
        fl::u16 idx = mCtx->xyMapFn(x, y, mCtx->xyMapUserData);
        mCtx->leds[idx] = CRGB(pixel.red, pixel.green, pixel.blue);
    }

    fl::u16 xyMap(fl::u16 x, fl::u16 y) override {
        return mCtx->xyMapFn(x, y, mCtx->xyMapUserData);
    }
};

inline Context::~Context() {
    delete mEngine;
}

// Initialize context with grid dimensions
inline void init(Context &ctx, int w, int h) {
    if (!ctx.mEngine) {
        ctx.mEngine = new Context::Engine(&ctx);
    }
    ctx.num_x = w;
    ctx.num_y = h;
    ctx.mEngine->init(w, h);
}

// Set time for deterministic rendering
inline void setTime(Context &ctx, fl::u32 t) {
    ctx.currentTime = t;
    if (ctx.mEngine) {
        ctx.mEngine->setTime(t);
    }
}

// ============================================================
// Animation free functions (Visualizers)
// Each delegates to the corresponding ANIMartRIX method.
// ============================================================

inline void Rotating_Blob(Context &ctx) {
    auto *e = ctx.mEngine;
    e->get_ready();

    e->timings.master_speed = 0.01;
    e->timings.ratio[0] = 0.1;
    e->timings.ratio[1] = 0.03;
    e->timings.ratio[2] = 0.03;
    e->timings.ratio[3] = 0.03;

    e->timings.offset[1] = 10;
    e->timings.offset[2] = 20;
    e->timings.offset[3] = 30;

    e->calculate_oscillators(e->timings);

    for (int x = 0; x < e->num_x; x++) {
        for (int y = 0; y < e->num_y; y++) {
            e->animation.scale_x = 0.05;
            e->animation.scale_y = 0.05;
            e->animation.offset_x = 0;
            e->animation.offset_y = 0;
            e->animation.offset_z = 100;
            e->animation.angle = e->polar_theta[x][y] + e->move.radial[0];
            e->animation.dist = e->distance[x][y];
            e->animation.z = e->move.linear[0];
            e->animation.low_limit = -1;
            float show1 = e->render_value(e->animation);

            e->animation.angle =
                e->polar_theta[x][y] - e->move.radial[1] + show1 / 512.0;
            e->animation.dist = e->distance[x][y] * show1 / 255.0;
            e->animation.low_limit = 0;
            e->animation.z = e->move.linear[1];
            float show2 = e->render_value(e->animation);

            e->animation.angle =
                e->polar_theta[x][y] - e->move.radial[2] + show1 / 512.0;
            e->animation.dist = e->distance[x][y] * show1 / 220.0;
            e->animation.z = e->move.linear[2];
            float show3 = e->render_value(e->animation);

            e->animation.angle =
                e->polar_theta[x][y] - e->move.radial[3] + show1 / 512.0;
            e->animation.dist = e->distance[x][y] * show1 / 200.0;
            e->animation.z = e->move.linear[3];
            float show4 = e->render_value(e->animation);

            e->pixel.red = (show2 + show4) / 2;
            e->pixel.green = show3 / 6;
            e->pixel.blue = 0;

            e->pixel = e->rgb_sanity_check(e->pixel);
            e->setPixelColorInternal(x, y, e->pixel);
        }
    }
}
inline void Chasing_Spirals(Context &ctx) {
    auto *e = ctx.mEngine;
    e->get_ready();

    e->timings.master_speed = 0.01;
    e->timings.ratio[0] = 0.1;
    e->timings.ratio[1] = 0.13;
    e->timings.ratio[2] = 0.16;

    e->timings.offset[1] = 10;
    e->timings.offset[2] = 20;
    e->timings.offset[3] = 30;

    e->calculate_oscillators(e->timings);

    for (int x = 0; x < e->num_x; x++) {
        for (int y = 0; y < e->num_y; y++) {
            e->animation.angle =
                3 * e->polar_theta[x][y] + e->move.radial[0] - e->distance[x][y] / 3;
            e->animation.dist = e->distance[x][y];
            e->animation.scale_z = 0.1;
            e->animation.scale_y = 0.1;
            e->animation.scale_x = 0.1;
            e->animation.offset_x = e->move.linear[0];
            e->animation.offset_y = 0;
            e->animation.offset_z = 0;
            e->animation.z = 0;
            float show1 = e->render_value(e->animation);

            e->animation.angle =
                3 * e->polar_theta[x][y] + e->move.radial[1] - e->distance[x][y] / 3;
            e->animation.dist = e->distance[x][y];
            e->animation.offset_x = e->move.linear[1];
            float show2 = e->render_value(e->animation);

            e->animation.angle =
                3 * e->polar_theta[x][y] + e->move.radial[2] - e->distance[x][y] / 3;
            e->animation.dist = e->distance[x][y];
            e->animation.offset_x = e->move.linear[2];
            float show3 = e->render_value(e->animation);

            float radius = e->radial_filter_radius;
            float radial_filter = (radius - e->distance[x][y]) / radius;

            e->pixel.red = 3 * show1 * radial_filter;
            e->pixel.green = show2 * radial_filter / 2;
            e->pixel.blue = show3 * radial_filter / 4;

            e->pixel = e->rgb_sanity_check(e->pixel);
            e->setPixelColorInternal(x, y, e->pixel);
        }
    }
}

inline void Rings(Context &ctx) {
    auto *e = ctx.mEngine;
    e->get_ready();

    e->timings.master_speed = 0.01;
    e->timings.ratio[0] = 1;
    e->timings.ratio[1] = 1.1;
    e->timings.ratio[2] = 1.2;

    e->timings.offset[1] = 100;
    e->timings.offset[2] = 200;
    e->timings.offset[3] = 300;

    e->calculate_oscillators(e->timings);

    for (int x = 0; x < e->num_x; x++) {
        for (int y = 0; y < e->num_y; y++) {
            e->animation.angle = 5;
            e->animation.scale_x = 0.2;
            e->animation.scale_y = 0.2;
            e->animation.scale_z = 1;
            e->animation.dist = e->distance[x][y];
            e->animation.offset_y = -e->move.linear[0];
            e->animation.offset_x = 0;
            float show1 = e->render_value(e->animation);

            e->animation.angle = 10;
            e->animation.dist = e->distance[x][y];
            e->animation.offset_y = -e->move.linear[1];
            float show2 = e->render_value(e->animation);

            e->animation.angle = 12;
            e->animation.dist = e->distance[x][y];
            e->animation.offset_y = -e->move.linear[2];
            float show3 = e->render_value(e->animation);

            e->pixel.red = show1;
            e->pixel.green = show2 / 4;
            e->pixel.blue = show3 / 4;

            e->pixel = e->rgb_sanity_check(e->pixel);
            e->setPixelColorInternal(x, y, e->pixel);
        }
    }
}
inline void Waves(Context &ctx) {
    auto *e = ctx.mEngine;
    e->get_ready();

    e->timings.master_speed = 0.01;
    e->timings.ratio[0] = 2;
    e->timings.ratio[1] = 2.1;
    e->timings.ratio[2] = 1.2;

    e->timings.offset[1] = 100;
    e->timings.offset[2] = 200;
    e->timings.offset[3] = 300;

    e->calculate_oscillators(e->timings);

    for (int x = 0; x < e->num_x; x++) {
        for (int y = 0; y < e->num_y; y++) {
            e->animation.angle = e->polar_theta[x][y];
            e->animation.scale_x = 0.1;
            e->animation.scale_y = 0.1;
            e->animation.scale_z = 0.1;
            e->animation.dist = e->distance[x][y];
            e->animation.offset_y = 0;
            e->animation.offset_x = 0;
            e->animation.z = 2 * e->distance[x][y] - e->move.linear[0];
            float show1 = e->render_value(e->animation);

            e->animation.angle = e->polar_theta[x][y];
            e->animation.dist = e->distance[x][y];
            e->animation.z = 2 * e->distance[x][y] - e->move.linear[1];
            float show2 = e->render_value(e->animation);

            e->pixel.red = show1;
            e->pixel.green = 0;
            e->pixel.blue = show2;

            e->pixel = e->rgb_sanity_check(e->pixel);
            e->setPixelColorInternal(x, y, e->pixel);
        }
    }
}
inline void Center_Field(Context &ctx) {
    auto *e = ctx.mEngine;
    e->get_ready();

    e->timings.master_speed = 0.01;
    e->timings.ratio[0] = 1;
    e->timings.ratio[1] = 1.1;
    e->timings.ratio[2] = 1.2;

    e->timings.offset[1] = 100;
    e->timings.offset[2] = 200;
    e->timings.offset[3] = 300;

    e->calculate_oscillators(e->timings);

    for (int x = 0; x < e->num_x; x++) {
        for (int y = 0; y < e->num_y; y++) {
            e->animation.angle = e->polar_theta[x][y];
            e->animation.scale_x = 0.07;
            e->animation.scale_y = 0.07;
            e->animation.scale_z = 0.1;
            e->animation.dist = 5 * fl::sqrtf(e->distance[x][y]);
            e->animation.offset_y = e->move.linear[0];
            e->animation.offset_x = 0;
            e->animation.z = 0;
            float show1 = e->render_value(e->animation);

            e->animation.angle = e->polar_theta[x][y];
            e->animation.scale_x = 0.07;
            e->animation.scale_y = 0.07;
            e->animation.scale_z = 0.1;
            e->animation.dist = 4 * fl::sqrtf(e->distance[x][y]);
            e->animation.offset_y = e->move.linear[0];
            e->animation.offset_x = 0;
            e->animation.z = 0;
            float show2 = e->render_value(e->animation);

            e->pixel.red = show1;
            e->pixel.green = show2;
            e->pixel.blue = 0;

            e->pixel = e->rgb_sanity_check(e->pixel);
            e->setPixelColorInternal(x, y, e->pixel);
        }
    }
}
inline void Distance_Experiment(Context &ctx) {
    auto *e = ctx.mEngine;
    e->get_ready();

    e->timings.master_speed = 0.01;
    e->timings.ratio[0] = 0.2;
    e->timings.ratio[1] = 0.13;
    e->timings.ratio[2] = 0.012;

    e->timings.offset[1] = 100;
    e->timings.offset[2] = 200;
    e->timings.offset[3] = 300;

    e->calculate_oscillators(e->timings);

    for (int x = 0; x < e->num_x; x++) {
        for (int y = 0; y < e->num_y; y++) {
            e->animation.dist = fl::powf(e->distance[x][y], 0.5);
            e->animation.angle = e->polar_theta[x][y] + e->move.radial[0];
            e->animation.scale_x = 0.07;
            e->animation.scale_y = 0.07;
            e->animation.scale_z = 0.1;
            e->animation.offset_y = e->move.linear[0];
            e->animation.offset_x = 0;
            e->animation.offset_z = 0;
            e->animation.z = 0;
            float show1 = e->render_value(e->animation);

            e->animation.dist = fl::powf(e->distance[x][y], 0.6);
            e->animation.angle = e->polar_theta[x][y] + e->move.noise_angle[2];
            e->animation.scale_x = 0.07;
            e->animation.scale_y = 0.07;
            e->animation.scale_z = 0.1;
            e->animation.offset_y = e->move.linear[1];
            e->animation.offset_x = 0;
            e->animation.offset_z = 0;
            e->animation.z = 0;
            float show2 = e->render_value(e->animation);

            e->pixel.red = show1 + show2;
            e->pixel.green = show2;
            e->pixel.blue = 0;

            e->pixel = e->rgb_sanity_check(e->pixel);
            e->setPixelColorInternal(x, y, e->pixel);
        }
    }
}
inline void Caleido1(Context &ctx) {
    auto *e = ctx.mEngine;
    e->get_ready();

    e->timings.master_speed = 0.003;
    e->timings.ratio[0] = 0.02;
    e->timings.ratio[1] = 0.03;
    e->timings.ratio[2] = 0.04;
    e->timings.ratio[3] = 0.05;
    e->timings.ratio[4] = 0.6;
    e->timings.offset[0] = 0;
    e->timings.offset[1] = 100;
    e->timings.offset[2] = 200;
    e->timings.offset[3] = 300;
    e->timings.offset[4] = 400;

    e->calculate_oscillators(e->timings);

    for (int x = 0; x < e->num_x; x++) {
        for (int y = 0; y < e->num_y; y++) {
            e->animation.dist = e->distance[x][y] * (2 + e->move.directional[0]) / 3;
            e->animation.angle = 3 * e->polar_theta[x][y] +
                                 3 * e->move.noise_angle[0] + e->move.radial[4];
            e->animation.scale_x = 0.1;
            e->animation.scale_y = 0.1;
            e->animation.scale_z = 0.1;
            e->animation.offset_y = 2 * e->move.linear[0];
            e->animation.offset_x = 0;
            e->animation.offset_z = 0;
            e->animation.z = e->move.linear[0];
            float show1 = e->render_value(e->animation);

            e->animation.dist = e->distance[x][y] * (2 + e->move.directional[1]) / 3;
            e->animation.angle = 4 * e->polar_theta[x][y] +
                                 3 * e->move.noise_angle[1] + e->move.radial[4];
            e->animation.offset_x = 2 * e->move.linear[1];
            e->animation.z = e->move.linear[1];
            float show2 = e->render_value(e->animation);

            e->animation.dist = e->distance[x][y] * (2 + e->move.directional[2]) / 3;
            e->animation.angle = 5 * e->polar_theta[x][y] +
                                 3 * e->move.noise_angle[2] + e->move.radial[4];
            e->animation.offset_y = 2 * e->move.linear[2];
            e->animation.z = e->move.linear[2];
            float show3 = e->render_value(e->animation);

            e->animation.dist = e->distance[x][y] * (2 + e->move.directional[3]) / 3;
            e->animation.angle = 4 * e->polar_theta[x][y] +
                                 3 * e->move.noise_angle[3] + e->move.radial[4];
            e->animation.offset_x = 2 * e->move.linear[3];
            e->animation.z = e->move.linear[3];
            float show4 = e->render_value(e->animation);

            e->pixel.red = show1;
            e->pixel.green = show3 * e->distance[x][y] / 10;
            e->pixel.blue = (show2 + show4) / 2;

            e->pixel = e->rgb_sanity_check(e->pixel);
            e->setPixelColorInternal(x, y, e->pixel);
        }
    }
}
inline void Caleido2(Context &ctx) {
    auto *e = ctx.mEngine;
    e->get_ready();

    e->timings.master_speed = 0.002;
    e->timings.ratio[0] = 0.02;
    e->timings.ratio[1] = 0.03;
    e->timings.ratio[2] = 0.04;
    e->timings.ratio[3] = 0.05;
    e->timings.ratio[4] = 0.6;
    e->timings.offset[0] = 0;
    e->timings.offset[1] = 100;
    e->timings.offset[2] = 200;
    e->timings.offset[3] = 300;
    e->timings.offset[4] = 400;

    e->calculate_oscillators(e->timings);

    for (int x = 0; x < e->num_x; x++) {
        for (int y = 0; y < e->num_y; y++) {
            e->animation.dist = e->distance[x][y] * (2 + e->move.directional[0]) / 3;
            e->animation.angle = 2 * e->polar_theta[x][y] +
                                 3 * e->move.noise_angle[0] + e->move.radial[4];
            e->animation.scale_x = 0.1;
            e->animation.scale_y = 0.1;
            e->animation.scale_z = 0.1;
            e->animation.offset_y = 2 * e->move.linear[0];
            e->animation.offset_x = 0;
            e->animation.offset_z = 0;
            e->animation.z = e->move.linear[0];
            float show1 = e->render_value(e->animation);

            e->animation.dist = e->distance[x][y] * (2 + e->move.directional[1]) / 3;
            e->animation.angle = 2 * e->polar_theta[x][y] +
                                 3 * e->move.noise_angle[1] + e->move.radial[4];
            e->animation.offset_x = 2 * e->move.linear[1];
            e->animation.z = e->move.linear[1];
            float show2 = e->render_value(e->animation);

            e->animation.dist = e->distance[x][y] * (2 + e->move.directional[2]) / 3;
            e->animation.angle = 2 * e->polar_theta[x][y] +
                                 3 * e->move.noise_angle[2] + e->move.radial[4];
            e->animation.offset_y = 2 * e->move.linear[2];
            e->animation.z = e->move.linear[2];
            float show3 = e->render_value(e->animation);

            e->animation.dist = e->distance[x][y] * (2 + e->move.directional[3]) / 3;
            e->animation.angle = 2 * e->polar_theta[x][y] +
                                 3 * e->move.noise_angle[3] + e->move.radial[4];
            e->animation.offset_x = 2 * e->move.linear[3];
            e->animation.z = e->move.linear[3];
            float show4 = e->render_value(e->animation);

            e->pixel.red = show1;
            e->pixel.green = show3 * e->distance[x][y] / 10;
            e->pixel.blue = (show2 + show4) / 2;

            e->pixel = e->rgb_sanity_check(e->pixel);
            e->setPixelColorInternal(x, y, e->pixel);
        }
    }
}
inline void Caleido3(Context &ctx) {
    auto *e = ctx.mEngine;
    e->get_ready();

    e->timings.master_speed = 0.004;
    e->timings.ratio[0] = 0.02;
    e->timings.ratio[1] = 0.03;
    e->timings.ratio[2] = 0.04;
    e->timings.ratio[3] = 0.05;
    e->timings.ratio[4] = 0.6;
    e->timings.offset[0] = 0;
    e->timings.offset[1] = 100;
    e->timings.offset[2] = 200;
    e->timings.offset[3] = 300;
    e->timings.offset[4] = 400;

    e->calculate_oscillators(e->timings);

    for (int x = 0; x < e->num_x; x++) {
        for (int y = 0; y < e->num_y; y++) {
            e->animation.dist = e->distance[x][y] * (2 + e->move.directional[0]) / 3;
            e->animation.angle = 2 * e->polar_theta[x][y] +
                                 3 * e->move.noise_angle[0] + e->move.radial[4];
            e->animation.scale_x = 0.1;
            e->animation.scale_y = 0.1;
            e->animation.scale_z = 0.1;
            e->animation.offset_y = 2 * e->move.linear[0];
            e->animation.offset_x = 2 * e->move.linear[1];
            e->animation.offset_z = 0;
            e->animation.z = e->move.linear[0];
            float show1 = e->render_value(e->animation);

            e->animation.dist = e->distance[x][y] * (2 + e->move.directional[1]) / 3;
            e->animation.angle = 2 * e->polar_theta[x][y] +
                                 3 * e->move.noise_angle[1] + e->move.radial[4];
            e->animation.offset_x = 2 * e->move.linear[1];
            e->animation.offset_y = show1 / 20.0;
            e->animation.z = e->move.linear[1];
            float show2 = e->render_value(e->animation);

            e->animation.dist = e->distance[x][y] * (2 + e->move.directional[2]) / 3;
            e->animation.angle = 2 * e->polar_theta[x][y] +
                                 3 * e->move.noise_angle[2] + e->move.radial[4];
            e->animation.offset_y = 2 * e->move.linear[2];
            e->animation.offset_x = show2 / 20.0;
            e->animation.z = e->move.linear[2];
            float show3 = e->render_value(e->animation);

            e->animation.dist = e->distance[x][y] * (2 + e->move.directional[3]) / 3;
            e->animation.angle = 2 * e->polar_theta[x][y] +
                                 3 * e->move.noise_angle[3] + e->move.radial[4];
            e->animation.offset_x = 2 * e->move.linear[3];
            e->animation.offset_y = show3 / 20.0;
            e->animation.z = e->move.linear[3];
            float show4 = e->render_value(e->animation);

            float radius = e->radial_filter_radius;

            e->pixel.red = show1 * (y + 1) / e->num_y;
            e->pixel.green = show3 * e->distance[x][y] / 10;
            e->pixel.blue = (show2 + show4) / 2;
            if (e->distance[x][y] > radius) {
                e->pixel.red = 0;
                e->pixel.green = 0;
                e->pixel.blue = 0;
            }

            e->pixel = e->rgb_sanity_check(e->pixel);
            e->setPixelColorInternal(x, y, e->pixel);
        }
    }
}
inline void Lava1(Context &ctx) {
    auto *e = ctx.mEngine;
    e->get_ready();

    e->timings.master_speed = 0.0015;
    e->timings.ratio[0] = 4;
    e->timings.ratio[1] = 1;
    e->timings.ratio[2] = 1;
    e->timings.ratio[3] = 0.05;
    e->timings.ratio[4] = 0.6;
    e->timings.offset[0] = 0;
    e->timings.offset[1] = 100;
    e->timings.offset[2] = 200;
    e->timings.offset[3] = 300;
    e->timings.offset[4] = 400;

    e->calculate_oscillators(e->timings);

    for (int x = 0; x < e->num_x; x++) {
        for (int y = 0; y < e->num_y; y++) {
            e->animation.dist = e->distance[x][y] * 0.8;
            e->animation.angle = e->polar_theta[x][y];
            e->animation.scale_x = 0.15;
            e->animation.scale_y = 0.12;
            e->animation.scale_z = 0.01;
            e->animation.offset_y = -e->move.linear[0];
            e->animation.offset_x = 0;
            e->animation.offset_z = 0;
            e->animation.z = 30;
            float show1 = e->render_value(e->animation);

            e->animation.offset_y = -e->move.linear[1];
            e->animation.scale_x = 0.15;
            e->animation.scale_y = 0.12;
            e->animation.offset_x = show1 / 100;
            e->animation.offset_y += show1 / 100;
            float show2 = e->render_value(e->animation);

            e->animation.offset_y = -e->move.linear[2];
            e->animation.scale_x = 0.15;
            e->animation.scale_y = 0.12;
            e->animation.offset_x = show2 / 100;
            e->animation.offset_y += show2 / 100;
            float show3 = e->render_value(e->animation);

            float linear = (y) / (e->num_y - 1.f);

            e->pixel.red = linear * show2;
            e->pixel.green = 0.1 * linear * (show2 - show3);
            e->pixel.blue = 0;

            e->pixel = e->rgb_sanity_check(e->pixel);
            e->setPixelColorInternal(x, y, e->pixel);
        }
    }
}
inline void Scaledemo1(Context &ctx) {
    auto *e = ctx.mEngine;
    e->get_ready();

    e->timings.master_speed = 0.000001;
    e->timings.ratio[0] = 0.4;
    e->timings.ratio[1] = 0.32;
    e->timings.ratio[2] = 0.10;
    e->timings.ratio[3] = 0.05;
    e->timings.ratio[4] = 0.6;
    e->timings.offset[0] = 0;
    e->timings.offset[1] = 100;
    e->timings.offset[2] = 200;
    e->timings.offset[3] = 300;
    e->timings.offset[4] = 400;

    e->calculate_oscillators(e->timings);

    for (int x = 0; x < e->num_x; x++) {
        for (int y = 0; y < e->num_y; y++) {
            e->animation.dist = 0.3 * e->distance[x][y] * 0.8;
            e->animation.angle = 3 * e->polar_theta[x][y] + e->move.radial[2];
            e->animation.scale_x = 0.1 + (e->move.noise_angle[0]) / 10;
            e->animation.scale_y = 0.1 + (e->move.noise_angle[1]) / 10;
            e->animation.scale_z = 0.01;
            e->animation.offset_y = 0;
            e->animation.offset_x = 0;
            e->animation.offset_z = 100 * e->move.linear[0];
            e->animation.z = 30;
            float show1 = e->render_value(e->animation);

            e->animation.angle = 3;
            float show2 = e->render_value(e->animation);

            float dist = 1;
            e->pixel.red = show1 * dist;
            e->pixel.green = (show1 - show2) * dist * 0.3;
            e->pixel.blue = (show2 - show1) * dist;

            if (e->distance[x][y] > 16) {
                e->pixel.red = 0;
                e->pixel.green = 0;
                e->pixel.blue = 0;
            }

            e->pixel = e->rgb_sanity_check(e->pixel);
            e->setPixelColorInternal(x, y, e->pixel);
        }
    }
}
inline void Yves(Context &ctx) {
    auto *e = ctx.mEngine;
    e->get_ready();

    e->a = fl::micros();

    e->timings.master_speed = 0.001;
    e->timings.ratio[0] = 3;
    e->timings.ratio[1] = 2;
    e->timings.ratio[2] = 1;
    e->timings.ratio[3] = 0.13;
    e->timings.ratio[4] = 0.15;
    e->timings.ratio[5] = 0.03;
    e->timings.ratio[6] = 0.025;
    e->timings.offset[0] = 0;
    e->timings.offset[1] = 100;
    e->timings.offset[2] = 200;
    e->timings.offset[3] = 300;
    e->timings.offset[4] = 400;
    e->timings.offset[5] = 500;
    e->timings.offset[6] = 600;

    e->calculate_oscillators(e->timings);

    for (int x = 0; x < e->num_x; x++) {
        for (int y = 0; y < e->num_y; y++) {
            e->animation.dist = e->distance[x][y];
            e->animation.angle =
                e->polar_theta[x][y] + 2 * PI + e->move.noise_angle[5];
            e->animation.scale_x = 0.08;
            e->animation.scale_y = 0.08;
            e->animation.scale_z = 0.08;
            e->animation.offset_y = -e->move.linear[0];
            e->animation.offset_x = 0;
            e->animation.offset_z = 0;
            e->animation.z = 0;
            float show1 = e->render_value(e->animation);

            e->animation.dist = e->distance[x][y];
            e->animation.angle =
                e->polar_theta[x][y] + 2 * PI + e->move.noise_angle[6];
            ;
            e->animation.scale_x = 0.08;
            e->animation.scale_y = 0.08;
            e->animation.scale_z = 0.08;
            e->animation.offset_y = -e->move.linear[1];
            e->animation.offset_x = 0;
            e->animation.offset_z = 0;
            e->animation.z = 0;
            float show2 = e->render_value(e->animation);

            e->animation.angle = e->polar_theta[x][y] + show1 / 100 +
                                 e->move.noise_angle[3] + e->move.noise_angle[4];
            e->animation.dist = e->distance[x][y] + show2 / 50;
            e->animation.offset_y = -e->move.linear[2];

            e->animation.offset_y += show1 / 100;
            e->animation.offset_x += show2 / 100;

            float show3 = e->render_value(e->animation);

            e->animation.offset_y = 0;
            e->animation.offset_x = 0;

            float show4 = e->render_value(e->animation);

            e->pixel.red = show3;
            e->pixel.green = show3 * show4 / 255;
            e->pixel.blue = 0;

            e->pixel = e->rgb_sanity_check(e->pixel);
            e->setPixelColorInternal(x, y, e->pixel);
        }
    }
}
inline void Spiralus(Context &ctx) {
    auto *e = ctx.mEngine;
    e->get_ready();

    e->timings.master_speed = 0.0011;
    e->timings.ratio[0] = 1.5;
    e->timings.ratio[1] = 2.3;
    e->timings.ratio[2] = 3;
    e->timings.ratio[3] = 0.05;
    e->timings.ratio[4] = 0.2;
    e->timings.ratio[5] = 0.03;
    e->timings.ratio[6] = 0.025;
    e->timings.ratio[7] = 0.021;
    e->timings.ratio[8] = 0.027;
    e->timings.offset[0] = 0;
    e->timings.offset[1] = 100;
    e->timings.offset[2] = 200;
    e->timings.offset[3] = 300;
    e->timings.offset[4] = 400;
    e->timings.offset[5] = 500;
    e->timings.offset[6] = 600;

    e->calculate_oscillators(e->timings);

    for (int x = 0; x < e->num_x; x++) {
        for (int y = 0; y < e->num_y; y++) {
            e->animation.dist = e->distance[x][y];
            e->animation.angle = 2 * e->polar_theta[x][y] + e->move.noise_angle[5] +
                                 e->move.directional[3] * e->move.noise_angle[6] *
                                     e->animation.dist / 10;
            e->animation.scale_x = 0.08;
            e->animation.scale_y = 0.08;
            e->animation.scale_z = 0.02;
            e->animation.offset_y = -e->move.linear[0];
            e->animation.offset_x = 0;
            e->animation.offset_z = 0;
            e->animation.z = e->move.linear[1];
            float show1 = e->render_value(e->animation);

            e->animation.angle = 2 * e->polar_theta[x][y] + e->move.noise_angle[7] +
                                 e->move.directional[5] * e->move.noise_angle[8] *
                                     e->animation.dist / 10;
            e->animation.offset_y = -e->move.linear[1];
            e->animation.z = e->move.linear[2];
            float show2 = e->render_value(e->animation);

            e->animation.angle = 2 * e->polar_theta[x][y] + e->move.noise_angle[6] +
                                 e->move.directional[6] * e->move.noise_angle[7] *
                                     e->animation.dist / 10;
            e->animation.offset_y = e->move.linear[2];
            e->animation.z = e->move.linear[0];
            float show3 = e->render_value(e->animation);

            float f = 1;

            e->pixel.red = f * (show1 + show2);
            e->pixel.green = f * (show1 - show2);
            e->pixel.blue = f * (show3 - show1);

            e->pixel = e->rgb_sanity_check(e->pixel);
            e->setPixelColorInternal(x, y, e->pixel);
        }
    }
}
inline void Spiralus2(Context &ctx) {
    auto *e = ctx.mEngine;
    e->get_ready();

    e->timings.master_speed = 0.0015;
    e->timings.ratio[0] = 1.5;
    e->timings.ratio[1] = 2.3;
    e->timings.ratio[2] = 3;
    e->timings.ratio[3] = 0.05;
    e->timings.ratio[4] = 0.2;
    e->timings.ratio[5] = 0.05;
    e->timings.ratio[6] = 0.055;
    e->timings.ratio[7] = 0.06;
    e->timings.ratio[8] = 0.027;
    e->timings.offset[0] = 0;
    e->timings.offset[1] = 100;
    e->timings.offset[2] = 200;
    e->timings.offset[3] = 300;
    e->timings.offset[4] = 400;
    e->timings.offset[5] = 500;
    e->timings.offset[6] = 600;

    e->calculate_oscillators(e->timings);

    for (int x = 0; x < e->num_x; x++) {
        for (int y = 0; y < e->num_y; y++) {
            e->animation.dist = e->distance[x][y];
            e->animation.angle = 5 * e->polar_theta[x][y] + e->move.noise_angle[5] +
                                 e->move.directional[3] * e->move.noise_angle[6] *
                                     e->animation.dist / 10;
            e->animation.scale_x = 0.08;
            e->animation.scale_y = 0.08;
            e->animation.scale_z = 0.02;
            e->animation.offset_y = -e->move.linear[0];
            e->animation.offset_x = 0;
            e->animation.offset_z = 0;
            e->animation.z = e->move.linear[1];
            float show1 = e->render_value(e->animation);

            e->animation.angle = 6 * e->polar_theta[x][y] + e->move.noise_angle[7] +
                                 e->move.directional[5] * e->move.noise_angle[8] *
                                     e->animation.dist / 10;
            e->animation.offset_y = -e->move.linear[1];
            e->animation.z = e->move.linear[2];
            float show2 = e->render_value(e->animation);

            e->animation.angle = 6 * e->polar_theta[x][y] + e->move.noise_angle[6] +
                                 e->move.directional[6] * e->move.noise_angle[7] *
                                     e->animation.dist / 10;
            e->animation.offset_y = e->move.linear[2];
            e->animation.z = e->move.linear[0];
            e->animation.dist = e->distance[x][y] * 0.8;
            float show3 = e->render_value(e->animation);

            float f = 1;

            e->pixel.red = f * (show1 + show2);
            e->pixel.green = f * (show1 - show2);
            e->pixel.blue = f * (show3 - show1);

            e->pixel = e->rgb_sanity_check(e->pixel);
            e->setPixelColorInternal(x, y, e->pixel);
        }
    }
}
inline void Hot_Blob(Context &ctx) {
    auto *e = ctx.mEngine;
    e->get_ready();
    e->run_default_oscillators(0.001);

    for (int x = 0; x < e->num_x; x++) {
        for (int y = 0; y < e->num_y; y++) {
            e->animation.dist = e->distance[x][y];
            e->animation.angle = e->polar_theta[x][y];

            e->animation.scale_x = 0.07 + e->move.directional[0] * 0.002;
            e->animation.scale_y = 0.07;

            e->animation.offset_y = -e->move.linear[0];
            e->animation.offset_x = 0;
            e->animation.offset_z = 0;

            e->animation.z = 0;
            e->animation.low_limit = -1;
            float show1 = e->render_value(e->animation);

            e->animation.offset_y = -e->move.linear[1];
            float show3 = e->render_value(e->animation);

            e->animation.offset_x = show3 / 20;
            e->animation.offset_y = -e->move.linear[0] / 2 + show1 / 70;
            e->animation.low_limit = 0;
            float show2 = e->render_value(e->animation);

            e->animation.offset_x = show3 / 20;
            e->animation.offset_y = -e->move.linear[0] / 2 + show1 / 70;
            e->animation.z = 100;
            float show4 = e->render_value(e->animation);

            float radius = e->radial_filter_radius;
            float radial = (radius - e->animation.dist) / e->animation.dist;

            float linear = (y + 1) / (e->num_y - 1.f);

            e->pixel.red = radial * show2;
            e->pixel.green = linear * radial * 0.3 * (show2 - show4);
            e->pixel.blue = 0;

            e->pixel = e->rgb_sanity_check(e->pixel);
            e->setPixelColorInternal(x, y, e->pixel);
        }
    }
}
inline void Zoom(Context &ctx) {
    auto *e = ctx.mEngine;
    e->get_ready();

    e->run_default_oscillators();
    e->timings.master_speed = 0.003;
    e->calculate_oscillators(e->timings);

    for (int x = 0; x < e->num_x; x++) {
        for (int y = 0; y < e->num_y; y++) {
            e->animation.dist = (e->distance[x][y] * e->distance[x][y]) / 2;
            e->animation.angle = e->polar_theta[x][y];

            e->animation.scale_x = 0.005;
            e->animation.scale_y = 0.005;

            e->animation.offset_y = -10 * e->move.linear[0];
            e->animation.offset_x = 0;
            e->animation.offset_z = 0;

            e->animation.z = 0;
            e->animation.low_limit = 0;
            float show1 = e->render_value(e->animation);

            float linear = 1;

            e->pixel.red = show1 * linear;
            e->pixel.green = 0;
            e->pixel.blue = 0;

            e->pixel = e->rgb_sanity_check(e->pixel);
            e->setPixelColorInternal(x, y, e->pixel);
        }
    }
}
inline void Slow_Fade(Context &ctx) {
    auto *e = ctx.mEngine;
    e->get_ready();

    e->run_default_oscillators();
    e->timings.master_speed = 0.00005;
    e->calculate_oscillators(e->timings);

    for (int x = 0; x < e->num_x; x++) {
        for (int y = 0; y < e->num_y; y++) {
            e->animation.dist =
                fl::sqrtf(e->distance[x][y]) * 0.7 * (e->move.directional[0] + 1.5);
            e->animation.angle =
                e->polar_theta[x][y] - e->move.radial[0] + e->distance[x][y] / 5;

            e->animation.scale_x = 0.11;
            e->animation.scale_y = 0.11;

            e->animation.offset_y = -50 * e->move.linear[0];
            e->animation.offset_x = 0;
            e->animation.offset_z = 0;

            e->animation.z = e->move.linear[0];
            e->animation.low_limit = -0.1;
            e->animation.high_limit = 1;
            float show1 = e->render_value(e->animation);

            e->animation.dist = e->animation.dist * 1.1;
            e->animation.angle += e->move.noise_angle[0] / 10;
            float show2 = e->render_value(e->animation);

            e->animation.dist = e->animation.dist * 1.1;
            e->animation.angle += e->move.noise_angle[1] / 10;
            float show3 = e->render_value(e->animation);

            float radius = e->radial_filter_radius;
            float radial = (radius - e->distance[x][y]) / e->distance[x][y];

            e->pixel.red = radial * show1;
            e->pixel.green = radial * (show1 - show2) / 6;
            e->pixel.blue = radial * (show1 - show3) / 5;

            e->pixel = e->rgb_sanity_check(e->pixel);
            e->setPixelColorInternal(x, y, e->pixel);
        }
    }
}
inline void Polar_Waves(Context &ctx) {
    auto *e = ctx.mEngine;
    e->get_ready();

    e->timings.master_speed = 0.5;
    e->timings.ratio[0] = 0.0025;
    e->timings.ratio[1] = 0.0027;
    e->timings.ratio[2] = 0.0031;

    e->calculate_oscillators(e->timings);

    for (int x = 0; x < e->num_x; x++) {
        for (int y = 0; y < e->num_y; y++) {
            e->animation.dist = (e->distance[x][y]);
            e->animation.angle =
                e->polar_theta[x][y] - e->animation.dist * 0.1 + e->move.radial[0];
            e->animation.z = (e->animation.dist * 1.5) - 10 * e->move.linear[0];
            e->animation.scale_x = 0.15;
            e->animation.scale_y = 0.15;
            e->animation.offset_x = e->move.linear[0];

            float show1 = e->render_value(e->animation);
            e->animation.angle =
                e->polar_theta[x][y] - e->animation.dist * 0.1 + e->move.radial[1];
            e->animation.z = (e->animation.dist * 1.5) - 10 * e->move.linear[1];
            e->animation.offset_x = e->move.linear[1];

            float show2 = e->render_value(e->animation);
            e->animation.angle =
                e->polar_theta[x][y] - e->animation.dist * 0.1 + e->move.radial[2];
            e->animation.z = (e->animation.dist * 1.5) - 10 * e->move.linear[2];
            e->animation.offset_x = e->move.linear[2];

            float show3 = e->render_value(e->animation);

            float radius = e->radial_filter_radius;
            float radial = (radius - e->distance[x][y]) / e->distance[x][y];

            e->pixel.red = radial * show1;
            e->pixel.green = radial * show2;
            e->pixel.blue = radial * show3;

            e->pixel = e->rgb_sanity_check(e->pixel);
            e->setPixelColorInternal(x, y, e->pixel);
        }
    }
}
inline void RGB_Blobs(Context &ctx) {
    auto *e = ctx.mEngine;
    e->get_ready();

    e->timings.master_speed = 0.2;
    e->timings.ratio[0] = 0.0025;
    e->timings.ratio[1] = 0.0027;
    e->timings.ratio[2] = 0.0031;
    e->timings.ratio[3] = 0.0033;
    e->timings.ratio[4] = 0.0036;
    e->timings.ratio[5] = 0.0039;

    e->calculate_oscillators(e->timings);

    for (int x = 0; x < e->num_x; x++) {
        for (int y = 0; y < e->num_y; y++) {
            e->animation.dist = e->distance[x][y];
            e->animation.angle = e->polar_theta[x][y] + e->move.radial[0] +
                                 e->move.noise_angle[0] + e->move.noise_angle[3];
            e->animation.z = (fl::sqrtf(e->animation.dist));
            e->animation.scale_x = 0.1;
            e->animation.scale_y = 0.1;
            e->animation.offset_z = 10;
            e->animation.offset_x = 10 * e->move.linear[0];
            float show1 = e->render_value(e->animation);

            e->animation.angle = e->polar_theta[x][y] + e->move.radial[1] +
                                 e->move.noise_angle[1] + e->move.noise_angle[4];
            e->animation.offset_x = 11 * e->move.linear[1];
            e->animation.offset_z = 100;
            float show2 = e->render_value(e->animation);

            e->animation.angle = e->polar_theta[x][y] + e->move.radial[2] +
                                 e->move.noise_angle[2] + e->move.noise_angle[5];
            e->animation.offset_x = 12 * e->move.linear[2];
            e->animation.offset_z = 300;
            float show3 = e->render_value(e->animation);

            float radius = e->radial_filter_radius;
            float radial = (radius - e->distance[x][y]) / e->distance[x][y];

            e->pixel.red = radial * show1;
            e->pixel.green = radial * show2;
            e->pixel.blue = radial * show3;

            e->pixel = e->rgb_sanity_check(e->pixel);
            e->setPixelColorInternal(x, y, e->pixel);
        }
    }
}
inline void RGB_Blobs2(Context &ctx) {
    auto *e = ctx.mEngine;
    e->get_ready();

    e->timings.master_speed = 0.12;
    e->timings.ratio[0] = 0.0025;
    e->timings.ratio[1] = 0.0027;
    e->timings.ratio[2] = 0.0031;
    e->timings.ratio[3] = 0.0033;
    e->timings.ratio[4] = 0.0036;
    e->timings.ratio[5] = 0.0039;

    e->calculate_oscillators(e->timings);

    for (int x = 0; x < e->num_x; x++) {
        for (int y = 0; y < e->num_y; y++) {
            e->animation.dist = e->distance[x][y];
            e->animation.angle = e->polar_theta[x][y] + e->move.radial[0] +
                                 e->move.noise_angle[0] + e->move.noise_angle[3] +
                                 e->move.noise_angle[1];
            e->animation.z = (fl::sqrtf(e->animation.dist));
            e->animation.scale_x = 0.1;
            e->animation.scale_y = 0.1;
            e->animation.offset_z = 10;
            e->animation.offset_x = 10 * e->move.linear[0];
            float show1 = e->render_value(e->animation);

            e->animation.angle = e->polar_theta[x][y] + e->move.radial[1] +
                                 e->move.noise_angle[1] + e->move.noise_angle[4] +
                                 e->move.noise_angle[2];
            e->animation.offset_x = 11 * e->move.linear[1];
            e->animation.offset_z = 100;
            float show2 = e->render_value(e->animation);

            e->animation.angle = e->polar_theta[x][y] + e->move.radial[2] +
                                 e->move.noise_angle[2] + e->move.noise_angle[5] +
                                 e->move.noise_angle[3];
            e->animation.offset_x = 12 * e->move.linear[2];
            e->animation.offset_z = 300;
            float show3 = e->render_value(e->animation);

            float radius = e->radial_filter_radius;
            float radial = (radius - e->distance[x][y]) / e->distance[x][y];

            e->pixel.red = radial * (show1 - show3);
            e->pixel.green = radial * (show2 - show1);
            e->pixel.blue = radial * (show3 - show2);

            e->pixel = e->rgb_sanity_check(e->pixel);
            e->setPixelColorInternal(x, y, e->pixel);
        }
    }
}
inline void RGB_Blobs3(Context &ctx) {
    auto *e = ctx.mEngine;
    e->get_ready();

    e->timings.master_speed = 0.12;
    e->timings.ratio[0] = 0.0025;
    e->timings.ratio[1] = 0.0027;
    e->timings.ratio[2] = 0.0031;
    e->timings.ratio[3] = 0.0033;
    e->timings.ratio[4] = 0.0036;
    e->timings.ratio[5] = 0.0039;

    e->calculate_oscillators(e->timings);

    for (int x = 0; x < e->num_x; x++) {
        for (int y = 0; y < e->num_y; y++) {
            e->animation.dist = e->distance[x][y] + e->move.noise_angle[4];
            e->animation.angle = e->polar_theta[x][y] + e->move.radial[0] +
                                 e->move.noise_angle[0] + e->move.noise_angle[3] +
                                 e->move.noise_angle[1];
            e->animation.z = (fl::sqrtf(e->animation.dist));
            e->animation.scale_x = 0.1;
            e->animation.scale_y = 0.1;
            e->animation.offset_z = 10;
            e->animation.offset_x = 10 * e->move.linear[0];
            float show1 = e->render_value(e->animation);

            e->animation.angle = e->polar_theta[x][y] + e->move.radial[1] +
                                 e->move.noise_angle[1] + e->move.noise_angle[4] +
                                 e->move.noise_angle[2];
            e->animation.offset_x = 11 * e->move.linear[1];
            e->animation.offset_z = 100;
            float show2 = e->render_value(e->animation);

            e->animation.angle = e->polar_theta[x][y] + e->move.radial[2] +
                                 e->move.noise_angle[2] + e->move.noise_angle[5] +
                                 e->move.noise_angle[3];
            e->animation.offset_x = 12 * e->move.linear[2];
            e->animation.offset_z = 300;
            float show3 = e->render_value(e->animation);

            float radius = e->radial_filter_radius;
            float radial = (radius - e->distance[x][y]) / e->distance[x][y];

            e->pixel.red = radial * (show1 + show3) * 0.5 * e->animation.dist / 5;
            e->pixel.green = radial * (show2 + show1) * 0.5 * y / 15;
            e->pixel.blue = radial * (show3 + show2) * 0.5 * x / 15;

            e->pixel = e->rgb_sanity_check(e->pixel);
            e->setPixelColorInternal(x, y, e->pixel);
        }
    }
}
inline void RGB_Blobs4(Context &ctx) {
    auto *e = ctx.mEngine;
    e->get_ready();

    e->timings.master_speed = 0.02;
    e->timings.ratio[0] = 0.0025;
    e->timings.ratio[1] = 0.0027;
    e->timings.ratio[2] = 0.0031;
    e->timings.ratio[3] = 0.0033;
    e->timings.ratio[4] = 0.0036;
    e->timings.ratio[5] = 0.0039;

    e->calculate_oscillators(e->timings);

    for (int x = 0; x < e->num_x; x++) {
        for (int y = 0; y < e->num_y; y++) {
            e->animation.dist = e->distance[x][y] + e->move.noise_angle[4];
            e->animation.angle = e->polar_theta[x][y] + e->move.radial[0] +
                                 e->move.noise_angle[0] + e->move.noise_angle[3] +
                                 e->move.noise_angle[1];
            e->animation.z = 3 + fl::sqrtf(e->animation.dist);
            e->animation.scale_x = 0.1;
            e->animation.scale_y = 0.1;
            e->animation.offset_z = 10;
            e->animation.offset_x = 50 * e->move.linear[0];
            float show1 = e->render_value(e->animation);

            e->animation.angle = e->polar_theta[x][y] + e->move.radial[1] +
                                 e->move.noise_angle[1] + e->move.noise_angle[4] +
                                 e->move.noise_angle[2];
            e->animation.offset_x = 50 * e->move.linear[1];
            e->animation.offset_z = 100;
            float show2 = e->render_value(e->animation);

            e->animation.angle = e->polar_theta[x][y] + e->move.radial[2] +
                                 e->move.noise_angle[2] + e->move.noise_angle[5] +
                                 e->move.noise_angle[3];
            e->animation.offset_x = 50 * e->move.linear[2];
            e->animation.offset_z = 300;
            float show3 = e->render_value(e->animation);

            float radius = 23;
            float radial = (radius - e->distance[x][y]) / e->distance[x][y];

            e->pixel.red = radial * (show1 + show3) * 0.5 * e->animation.dist / 5;
            e->pixel.green = radial * (show2 + show1) * 0.5 * y / 15;
            e->pixel.blue = radial * (show3 + show2) * 0.5 * x / 15;

            e->pixel = e->rgb_sanity_check(e->pixel);
            e->setPixelColorInternal(x, y, e->pixel);
        }
    }
}
inline void RGB_Blobs5(Context &ctx) {
    auto *e = ctx.mEngine;
    e->get_ready();

    e->timings.master_speed = 0.02;
    e->timings.ratio[0] = 0.0025;
    e->timings.ratio[1] = 0.0027;
    e->timings.ratio[2] = 0.0031;
    e->timings.ratio[3] = 0.0033;
    e->timings.ratio[4] = 0.0036;
    e->timings.ratio[5] = 0.0039;

    e->calculate_oscillators(e->timings);

    for (int x = 0; x < e->num_x; x++) {
        for (int y = 0; y < e->num_y; y++) {
            e->animation.dist = e->distance[x][y] + e->move.noise_angle[4];
            e->animation.angle = e->polar_theta[x][y] + e->move.radial[0] +
                                 e->move.noise_angle[0] + e->move.noise_angle[3] +
                                 e->move.noise_angle[1];
            e->animation.z = 3 + fl::sqrtf(e->animation.dist);
            e->animation.scale_x = 0.05;
            e->animation.scale_y = 0.05;
            e->animation.offset_z = 10;
            e->animation.offset_x = 50 * e->move.linear[0];
            float show1 = e->render_value(e->animation);

            e->animation.angle = e->polar_theta[x][y] + e->move.radial[1] +
                                 e->move.noise_angle[1] + e->move.noise_angle[4] +
                                 e->move.noise_angle[2];
            e->animation.offset_x = 50 * e->move.linear[1];
            e->animation.offset_z = 100;
            float show2 = e->render_value(e->animation);

            e->animation.angle = e->polar_theta[x][y] + e->move.radial[2] +
                                 e->move.noise_angle[2] + e->move.noise_angle[5] +
                                 e->move.noise_angle[3];
            e->animation.offset_x = 50 * e->move.linear[2];
            e->animation.offset_z = 300;
            float show3 = e->render_value(e->animation);

            float radius = 23;
            float radial = (radius - e->distance[x][y]) / e->distance[x][y];

            e->pixel.red = radial * (show1 + show3) * 0.5 * e->animation.dist / 5;
            e->pixel.green = radial * (show2 + show1) * 0.5 * y / 15;
            e->pixel.blue = radial * (show3 + show2) * 0.5 * x / 15;

            e->pixel = e->rgb_sanity_check(e->pixel);
            e->setPixelColorInternal(x, y, e->pixel);
        }
    }
}
inline void Big_Caleido(Context &ctx) {
    auto *e = ctx.mEngine;
    e->get_ready();

    e->timings.master_speed = 0.02;
    e->timings.ratio[0] = 0.0025;
    e->timings.ratio[1] = 0.0027;
    e->timings.ratio[2] = 0.0031;
    e->timings.ratio[3] = 0.0033;
    e->timings.ratio[4] = 0.0036;
    e->timings.ratio[5] = 0.0039;

    e->calculate_oscillators(e->timings);

    for (int x = 0; x < e->num_x; x++) {
        for (int y = 0; y < e->num_y; y++) {
            e->animation.dist = e->distance[x][y];
            e->animation.angle = 5 * e->polar_theta[x][y] +
                                 5 * e->move.noise_angle[0] +
                                 e->animation.dist * 0.1;
            e->animation.z = 5;
            e->animation.scale_x = 0.05;
            e->animation.scale_y = 0.05;
            e->animation.offset_z = 50 * e->move.linear[0];
            e->animation.offset_x = 50 * e->move.noise_angle[0];
            e->animation.offset_y = 50 * e->move.noise_angle[1];
            float show1 = e->render_value(e->animation);

            e->animation.angle = 6 * e->polar_theta[x][y] +
                                 5 * e->move.noise_angle[1] +
                                 e->animation.dist * 0.15;
            e->animation.z = 5;
            e->animation.scale_x = 0.05;
            e->animation.scale_y = 0.05;
            e->animation.offset_z = 50 * e->move.linear[1];
            e->animation.offset_x = 50 * e->move.noise_angle[1];
            e->animation.offset_y = 50 * e->move.noise_angle[2];
            float show2 = e->render_value(e->animation);

            e->animation.angle = 5;
            e->animation.z = 5;
            e->animation.scale_x = 0.10;
            e->animation.scale_y = 0.10;
            e->animation.offset_z = 10 * e->move.linear[2];
            e->animation.offset_x = 10 * e->move.noise_angle[2];
            e->animation.offset_y = 10 * e->move.noise_angle[3];
            float show3 = e->render_value(e->animation);

            e->animation.angle = 15;
            e->animation.z = 15;
            e->animation.scale_x = 0.10;
            e->animation.scale_y = 0.10;
            e->animation.offset_z = 10 * e->move.linear[3];
            e->animation.offset_x = 10 * e->move.noise_angle[3];
            e->animation.offset_y = 10 * e->move.noise_angle[4];
            float show4 = e->render_value(e->animation);

            e->animation.angle = 2;
            e->animation.z = 15;
            e->animation.scale_x = 0.15;
            e->animation.scale_y = 0.15;
            e->animation.offset_z = 10 * e->move.linear[4];
            e->animation.offset_x = 10 * e->move.noise_angle[4];
            e->animation.offset_y = 10 * e->move.noise_angle[5];
            float show5 = e->render_value(e->animation);

            e->pixel.red = show1 - show4;
            e->pixel.green = show2 - show5;
            e->pixel.blue = show3 - show2 + show1;

            e->pixel = e->rgb_sanity_check(e->pixel);
            e->setPixelColorInternal(x, y, e->pixel);
        }
    }
}
inline void SpiralMatrix1(Context &ctx) {
    auto *e = ctx.mEngine;
    e->get_ready();

    e->timings.master_speed = 0.02;
    e->timings.ratio[0] = 0.0025;
    e->timings.ratio[1] = 0.0027;
    e->timings.ratio[2] = 0.0031;
    e->timings.ratio[3] = 0.0033;
    e->timings.ratio[4] = 0.0036;
    e->timings.ratio[5] = 0.0039;

    e->calculate_oscillators(e->timings);

    for (int x = 0; x < e->num_x / 2; x++) {
        for (int y = 0; y < e->num_y / 2; y++) {

            e->animation.dist = e->distance[x][y];
            e->animation.angle = e->polar_theta[x][y] + 5 * e->move.noise_angle[0];
            e->animation.z = 5;
            e->animation.scale_x = 0.1;
            e->animation.scale_y = 0.1;
            e->animation.offset_z = 50 * e->move.linear[0];
            e->animation.offset_x = 150 * e->move.directional[0];
            e->animation.offset_y = 150 * e->move.directional[1];
            float show1 = e->render_value(e->animation);

            e->animation.dist = e->distance[x][y];
            e->animation.angle = e->polar_theta[x][y] + 4 * e->move.noise_angle[1];
            e->animation.z = 15;
            e->animation.scale_x = 0.15;
            e->animation.scale_y = 0.15;
            e->animation.offset_z = 50 * e->move.linear[1];
            e->animation.offset_x = 150 * e->move.directional[1];
            e->animation.offset_y = 150 * e->move.directional[2];
            float show2 = e->render_value(e->animation);

            e->animation.dist = e->distance[x][y];
            e->animation.angle = e->polar_theta[x][y] + 5 * e->move.noise_angle[2];
            e->animation.z = 25;
            e->animation.scale_x = 0.1;
            e->animation.scale_y = 0.1;
            e->animation.offset_z = 50 * e->move.linear[2];
            e->animation.offset_x = 150 * e->move.directional[2];
            e->animation.offset_y = 150 * e->move.directional[3];
            float show3 = e->render_value(e->animation);

            e->animation.dist = e->distance[x][y];
            e->animation.angle = e->polar_theta[x][y] + 5 * e->move.noise_angle[3];
            e->animation.z = 35;
            e->animation.scale_x = 0.15;
            e->animation.scale_y = 0.15;
            e->animation.offset_z = 50 * e->move.linear[3];
            e->animation.offset_x = 150 * e->move.directional[3];
            e->animation.offset_y = 150 * e->move.directional[4];
            float show4 = e->render_value(e->animation);

            e->animation.dist = e->distance[x][y];
            e->animation.angle = e->polar_theta[x][y] + 5 * e->move.noise_angle[4];
            e->animation.z = 45;
            e->animation.scale_x = 0.2;
            e->animation.scale_y = 0.2;
            e->animation.offset_z = 50 * e->move.linear[4];
            e->animation.offset_x = 150 * e->move.directional[4];
            e->animation.offset_y = 150 * e->move.directional[5];
            float show5 = e->render_value(e->animation);

            e->pixel.red = show1 + show2;
            e->pixel.green = show3 + show4;
            e->pixel.blue = show5;

            e->pixel = e->rgb_sanity_check(e->pixel);
            e->setPixelColorInternal(x, y, e->pixel);

            e->setPixelColorInternal((e->num_x - 1) - x, y, e->pixel);
            e->setPixelColorInternal((e->num_x - 1) - x, (e->num_y - 1) - y, e->pixel);
            e->setPixelColorInternal(x, (e->num_y - 1) - y, e->pixel);
        }
    }
}
inline void SpiralMatrix2(Context &ctx) {
    auto *e = ctx.mEngine;
    e->get_ready();

    e->timings.master_speed = 0.03;
    e->timings.ratio[0] = 0.025;
    e->timings.ratio[1] = 0.027;
    e->timings.ratio[2] = 0.031;
    e->timings.ratio[3] = 0.0033;
    e->timings.ratio[4] = 0.0036;
    e->timings.ratio[5] = 0.0039;

    e->calculate_oscillators(e->timings);

    for (int x = 0; x < e->num_x; x++) {
        for (int y = 0; y < e->num_y; y++) {

            e->animation.dist = e->distance[x][y] * (e->move.directional[0]);
            e->animation.angle = e->polar_theta[x][y] + e->move.radial[0];
            e->animation.z = 5;
            e->animation.scale_x = 0.09;
            e->animation.scale_y = 0.09;
            e->animation.offset_z = 5 * e->move.linear[0];
            e->animation.offset_x = 0;
            e->animation.offset_y = 0;
            float show1 = e->render_value(e->animation);

            e->animation.dist = e->distance[x][y] * e->move.directional[1];
            e->animation.angle = e->polar_theta[x][y] + e->move.radial[1];
            e->animation.z = 50;
            e->animation.scale_x = 0.07;
            e->animation.scale_y = 0.07;
            e->animation.offset_z = 5 * e->move.linear[1];
            e->animation.offset_x = 0;
            e->animation.offset_y = 0;
            float show2 = e->render_value(e->animation);

            e->animation.dist = e->distance[x][y] * e->move.directional[2];
            e->animation.angle = e->polar_theta[x][y] + e->move.radial[2];
            e->animation.z = 500;
            e->animation.scale_x = 0.05;
            e->animation.scale_y = 0.05;
            e->animation.offset_z = 5 * e->move.linear[2];
            e->animation.offset_x = 0;
            e->animation.offset_y = 0;
            float show3 = e->render_value(e->animation);

            e->pixel.red = show1;
            e->pixel.green = show2;
            e->pixel.blue = show3;

            e->pixel = e->rgb_sanity_check(e->pixel);

            e->setPixelColorInternal(x, y, e->pixel);
        }
    }
}
inline void SpiralMatrix3(Context &ctx) {
    auto *e = ctx.mEngine;
    e->get_ready();

    e->timings.master_speed = 0.02;
    e->timings.ratio[0] = 0.025;
    e->timings.ratio[1] = 0.027;
    e->timings.ratio[2] = 0.031;
    e->timings.ratio[3] = 0.0033;
    e->timings.ratio[4] = 0.0036;
    e->timings.ratio[5] = 0.0039;

    e->calculate_oscillators(e->timings);

    for (int x = 0; x < e->num_x; x++) {
        for (int y = 0; y < e->num_y; y++) {

            e->animation.dist = e->distance[x][y];
            e->animation.angle = e->polar_theta[x][y];
            e->animation.z = 5;
            e->animation.scale_x = 0.09;
            e->animation.scale_y = 0.09;
            e->animation.offset_z = 0;
            e->animation.offset_x = 0;
            e->animation.offset_y = -20 * e->move.linear[0];
            ;
            e->animation.low_limit = -1;
            e->animation.high_limit = 1;
            e->show1 = e->render_value(e->animation);

            e->animation.dist = e->distance[x][y];
            e->animation.angle = e->polar_theta[x][y];
            e->animation.z = 500;
            e->animation.scale_x = 0.09;
            e->animation.scale_y = 0.09;
            e->animation.offset_z = 0;
            e->animation.offset_x = 0;
            e->animation.offset_y = -20 * e->move.linear[0];
            ;
            e->animation.low_limit = -1;
            e->animation.high_limit = 1;
            e->show2 = e->render_value(e->animation);

            e->animation.dist = e->distance[x][y];
            e->animation.angle = e->polar_theta[x][y];
            e->animation.z = 50;
            e->animation.scale_x = 0.09;
            e->animation.scale_y = 0.09;
            e->animation.offset_z = 0;
            e->animation.offset_x = 500 + e->show1 / 20;
            e->animation.offset_y = -4 * e->move.linear[0] + e->show2 / 20;
            e->animation.low_limit = 0;
            e->animation.high_limit = 1;
            e->show3 = e->render_value(e->animation);

            e->animation.dist = e->distance[x][y];
            e->animation.angle = e->polar_theta[x][y];
            e->animation.z = 50;
            e->animation.scale_x = 0.09;
            e->animation.scale_y = 0.09;
            e->animation.offset_z = 0;
            e->animation.offset_x = 500 + e->show1 / 18;
            e->animation.offset_y = -4 * e->move.linear[0] + e->show2 / 18;
            e->animation.low_limit = 0;
            e->animation.high_limit = 1;
            e->show4 = e->render_value(e->animation);

            e->animation.dist = e->distance[x][y];
            e->animation.angle = e->polar_theta[x][y];
            e->animation.z = 50;
            e->animation.scale_x = 0.09;
            e->animation.scale_y = 0.09;
            e->animation.offset_z = 0;
            e->animation.offset_x = 500 + e->show1 / 19;
            e->animation.offset_y = -4 * e->move.linear[0] + e->show2 / 19;
            e->animation.low_limit = 0.3;
            e->animation.high_limit = 1;
            e->show5 = e->render_value(e->animation);

            e->pixel.red = e->show4;
            e->pixel.green = e->show3;
            e->pixel.blue = e->show5;

            e->pixel = e->rgb_sanity_check(e->pixel);

            e->setPixelColorInternal(x, y, e->pixel);
        }
    }
}
inline void SpiralMatrix4(Context &ctx) {
    auto *e = ctx.mEngine;
    e->get_ready();

    e->timings.master_speed = 0.02;
    e->timings.ratio[0] = 0.025;
    e->timings.ratio[1] = 0.027;
    e->timings.ratio[2] = 0.031;
    e->timings.ratio[3] = 0.0033;
    e->timings.ratio[4] = 0.0036;
    e->timings.ratio[5] = 0.0039;

    e->calculate_oscillators(e->timings);

    for (int x = 0; x < e->num_x; x++) {
        for (int y = 0; y < e->num_y; y++) {

            e->animation.dist = e->distance[x][y];
            e->animation.angle = e->polar_theta[x][y];
            e->animation.z = 5;
            e->animation.scale_x = 0.09;
            e->animation.scale_y = 0.09;
            e->animation.offset_z = 0;
            e->animation.offset_x = 0;
            e->animation.offset_y = -20 * e->move.linear[0];
            ;
            e->animation.low_limit = 0;
            e->animation.high_limit = 1;
            e->show1 = e->render_value(e->animation);

            e->animation.dist = e->distance[x][y];
            e->animation.angle = e->polar_theta[x][y];
            e->animation.z = 500;
            e->animation.scale_x = 0.09;
            e->animation.scale_y = 0.09;
            e->animation.offset_z = 0;
            e->animation.offset_x = 0;
            e->animation.offset_y = -40 * e->move.linear[0];
            ;
            e->animation.low_limit = 0;
            e->animation.high_limit = 1;
            e->show2 = e->render_value(e->animation);

            e->pixel.red = e->add(e->show2, e->show1);
            e->pixel.green = 0;
            e->pixel.blue = e->colordodge(e->show2, e->show1);

            e->pixel = e->rgb_sanity_check(e->pixel);

            e->setPixelColorInternal(x, y, e->pixel);
        }
    }
}
inline void SpiralMatrix5(Context &ctx) {
    auto *e = ctx.mEngine;
    e->get_ready();

    e->timings.master_speed = 0.03;
    e->timings.ratio[0] = 0.025;
    e->timings.ratio[1] = 0.027;
    e->timings.ratio[2] = 0.031;
    e->timings.ratio[3] = 0.0053;
    e->timings.ratio[4] = 0.0056;
    e->timings.ratio[5] = 0.0059;

    e->calculate_oscillators(e->timings);

    for (int x = 0; x < e->num_x; x++) {
        for (int y = 0; y < e->num_y; y++) {

            e->animation.dist = e->distance[x][y] * (e->move.directional[0]);
            e->animation.angle = e->polar_theta[x][y] + e->move.radial[0];
            e->animation.z = 5;
            e->animation.scale_x = 0.09;
            e->animation.scale_y = 0.09;
            e->animation.offset_z = 5 * e->move.linear[0];
            e->animation.offset_x = 0;
            e->animation.offset_y = 0;
            float show1 = e->render_value(e->animation);

            e->animation.dist = e->distance[x][y] * e->move.directional[1];
            e->animation.angle = e->polar_theta[x][y] + e->move.radial[1];
            e->animation.z = 50;
            e->animation.scale_x = 0.07;
            e->animation.scale_y = 0.07;
            e->animation.offset_z = 5 * e->move.linear[1];
            e->animation.offset_x = 0;
            e->animation.offset_y = 0;
            float show2 = e->render_value(e->animation);

            e->animation.dist = e->distance[x][y] * e->move.directional[2];
            e->animation.angle = e->polar_theta[x][y] + e->move.radial[2];
            e->animation.z = 500;
            e->animation.scale_x = 0.05;
            e->animation.scale_y = 0.05;
            e->animation.offset_z = 5 * e->move.linear[2];
            e->animation.offset_x = 0;
            e->animation.offset_y = 0;
            float show3 = e->render_value(e->animation);

            e->animation.dist = e->distance[x][y] * (e->move.directional[3]);
            e->animation.angle = e->polar_theta[x][y] + e->move.radial[3];
            e->animation.z = 5;
            e->animation.scale_x = 0.09;
            e->animation.scale_y = 0.09;
            e->animation.offset_z = 5 * e->move.linear[3];
            e->animation.offset_x = 0;
            e->animation.offset_y = 0;
            float show4 = e->render_value(e->animation);

            e->animation.dist = e->distance[x][y] * e->move.directional[4];
            e->animation.angle = e->polar_theta[x][y] + e->move.radial[4];
            e->animation.z = 50;
            e->animation.scale_x = 0.07;
            e->animation.scale_y = 0.07;
            e->animation.offset_z = 5 * e->move.linear[4];
            e->animation.offset_x = 0;
            e->animation.offset_y = 0;
            float show5 = e->render_value(e->animation);

            e->animation.dist = e->distance[x][y] * e->move.directional[5];
            e->animation.angle = e->polar_theta[x][y] + e->move.radial[5];
            e->animation.z = 500;
            e->animation.scale_x = 0.05;
            e->animation.scale_y = 0.05;
            e->animation.offset_z = 5 * e->move.linear[5];
            e->animation.offset_x = 0;
            e->animation.offset_y = 0;
            float show6 = e->render_value(e->animation);

            float radius = e->radial_filter_radius;
            float radial = (radius - e->distance[x][y]) / e->distance[x][y];

            e->pixel.red = radial * e->add(show1, show4);
            e->pixel.green = radial * e->colordodge(show2, show5);
            e->pixel.blue = radial * e->screen(show3, show6);

            e->pixel = e->rgb_sanity_check(e->pixel);

            e->setPixelColorInternal(x, y, e->pixel);
        }
    }
}
inline void SpiralMatrix6(Context &ctx) {
    auto *e = ctx.mEngine;
    e->get_ready();

    e->timings.master_speed = 0.03;
    e->timings.ratio[0] = 0.025;
    e->timings.ratio[1] = 0.027;
    e->timings.ratio[2] = 0.031;
    e->timings.ratio[3] = 0.0053;
    e->timings.ratio[4] = 0.0056;
    e->timings.ratio[5] = 0.0059;

    e->calculate_oscillators(e->timings);

    for (int x = 0; x < e->num_x; x++) {
        for (int y = 0; y < e->num_y; y++) {

            float s = 0.7;

            e->animation.dist = e->distance[x][y] * (e->move.directional[0]) * s;
            e->animation.angle = e->polar_theta[x][y] + e->move.radial[0];
            e->animation.z = 5;
            e->animation.scale_x = 0.09;
            e->animation.scale_y = 0.09;
            e->animation.offset_z = 5 * e->move.linear[0];
            e->animation.offset_x = 0;
            e->animation.offset_y = 0;
            float show1 = e->render_value(e->animation);

            e->animation.dist = e->distance[x][y] * e->move.directional[1] * s;
            e->animation.angle = e->polar_theta[x][y] + e->move.radial[1];
            e->animation.z = 50;
            e->animation.scale_x = 0.07;
            e->animation.scale_y = 0.07;
            e->animation.offset_z = 5 * e->move.linear[1];
            e->animation.offset_x = 0;
            e->animation.offset_y = 0;
            float show2 = e->render_value(e->animation);

            e->animation.dist = e->distance[x][y] * e->move.directional[2] * s;
            e->animation.angle = e->polar_theta[x][y] + e->move.radial[2];
            e->animation.z = 500;
            e->animation.scale_x = 0.05;
            e->animation.scale_y = 0.05;
            e->animation.offset_z = 5 * e->move.linear[2];
            e->animation.offset_x = 0;
            e->animation.offset_y = 0;
            float show3 = e->render_value(e->animation);

            e->animation.dist = e->distance[x][y] * (e->move.directional[3]) * s;
            e->animation.angle = e->polar_theta[x][y] + e->move.radial[3];
            e->animation.z = 5;
            e->animation.scale_x = 0.09;
            e->animation.scale_y = 0.09;
            e->animation.offset_z = 5 * e->move.linear[3];
            e->animation.offset_x = 0;
            e->animation.offset_y = 0;
            float show4 = e->render_value(e->animation);

            e->animation.dist = e->distance[x][y] * e->move.directional[4] * s;
            e->animation.angle = e->polar_theta[x][y] + e->move.radial[4];
            e->animation.z = 50;
            e->animation.scale_x = 0.07;
            e->animation.scale_y = 0.07;
            e->animation.offset_z = 5 * e->move.linear[4];
            e->animation.offset_x = 0;
            e->animation.offset_y = 0;
            float show5 = e->render_value(e->animation);

            e->animation.dist = e->distance[x][y] * e->move.directional[5] * s;
            e->animation.angle = e->polar_theta[x][y] + e->move.radial[5];
            e->animation.z = 500;
            e->animation.scale_x = 0.05;
            e->animation.scale_y = 0.05;
            e->animation.offset_z = 5 * e->move.linear[5];
            e->animation.offset_x = 0;
            e->animation.offset_y = 0;
            float show6 = e->render_value(e->animation);

            float radius = e->radial_filter_radius;
            float radial = (radius - e->distance[x][y]) / e->distance[x][y];

            e->show7 = e->screen(show1, show4);
            e->show8 = e->colordodge(show2, show5);
            e->show9 = e->screen(show3, show6);

            e->pixel.red = radial * (e->show7 + e->show8);
            e->pixel.green = 0;
            e->pixel.blue = radial * e->show9;

            e->pixel = e->rgb_sanity_check(e->pixel);

            e->setPixelColorInternal(x, y, e->pixel);
        }
    }
}
inline void SpiralMatrix8(Context &ctx) {
    auto *e = ctx.mEngine;
    e->get_ready();

    e->timings.master_speed = 0.005;
    e->timings.ratio[0] = 0.025;
    e->timings.ratio[1] = 0.027;
    e->timings.ratio[2] = 0.031;
    e->timings.ratio[3] = 0.0053;
    e->timings.ratio[4] = 0.0056;
    e->timings.ratio[5] = 0.01;

    e->calculate_oscillators(e->timings);

    for (int x = 0; x < e->num_x; x++) {
        for (int y = 0; y < e->num_y; y++) {

            e->animation.dist = e->distance[x][y];
            e->animation.angle = 2;
            e->animation.z = 5;
            e->animation.scale_x = 0.15;
            e->animation.scale_y = 0.15;
            e->animation.offset_z = 0;
            e->animation.offset_y = 50 * e->move.linear[0];
            e->animation.offset_x = 0;
            e->animation.low_limit = 0;
            float show1 = e->render_value(e->animation);

            e->animation.dist = e->distance[x][y];
            e->animation.angle = 2;
            e->animation.z = 150;
            e->animation.offset_x = -50 * e->move.linear[0];
            float show2 = e->render_value(e->animation);

            e->animation.dist = e->distance[x][y];
            e->animation.angle = 1;
            e->animation.z = 550;
            e->animation.scale_x = 0.15;
            e->animation.scale_y = 0.15;
            e->animation.offset_x = 0;
            e->animation.offset_y = -50 * e->move.linear[1];
            float show4 = e->render_value(e->animation);

            e->animation.dist = e->distance[x][y];
            e->animation.angle = 1;
            e->animation.z = 1250;
            e->animation.scale_x = 0.15;
            e->animation.scale_y = 0.15;
            e->animation.offset_x = 0;
            e->animation.offset_y = 50 * e->move.linear[1];
            float show5 = e->render_value(e->animation);

            e->show3 = e->add(show1, show2);
            e->show6 = e->screen(show4, show5);

            e->pixel.red = e->show3;
            e->pixel.green = 0;
            e->pixel.blue = e->show6;

            e->pixel = e->rgb_sanity_check(e->pixel);

            e->setPixelColorInternal(x, y, e->pixel);
        }
    }
}
inline void SpiralMatrix9(Context &ctx) {
    auto *e = ctx.mEngine;
    e->get_ready();

    e->timings.master_speed = 0.005;
    e->timings.ratio[0] = 0.025;
    e->timings.ratio[1] = 0.027;
    e->timings.ratio[2] = 0.031;
    e->timings.ratio[3] = 0.0053;
    e->timings.ratio[4] = 0.0056;
    e->timings.ratio[5] = 0.0059;

    e->calculate_oscillators(e->timings);

    for (int x = 0; x < e->num_x; x++) {
        for (int y = 0; y < e->num_y; y++) {

            e->animation.dist = e->distance[x][y];
            e->animation.angle = e->polar_theta[x][y];
            e->animation.z = 5;
            e->animation.scale_x = 0.09;
            e->animation.scale_y = 0.09;
            e->animation.offset_y = -30 * e->move.linear[0];
            e->animation.offset_z = 0;
            e->animation.offset_x = 0;
            e->animation.low_limit = -1;
            e->show1 = e->render_value(e->animation);

            e->animation.dist = e->distance[x][y];
            e->animation.angle = e->polar_theta[x][y];
            e->animation.z = 50;
            e->animation.scale_x = 0.09;
            e->animation.scale_y = 0.09;
            e->animation.offset_y = -30 * e->move.linear[1];
            e->animation.offset_z = 0;
            e->animation.offset_x = 0;
            e->animation.low_limit = -1;
            e->show2 = e->render_value(e->animation);

            e->animation.dist = e->distance[x][y];
            e->animation.angle = e->polar_theta[x][y] + 2 + (e->show1 / 255) * PI;
            e->animation.z = 5;
            e->animation.scale_x = 0.09;
            e->animation.scale_y = 0.09;
            e->animation.offset_y = -10 * e->move.linear[0];
            e->animation.offset_z = 0;
            e->animation.offset_x = 0;
            e->animation.low_limit = 0;
            e->show3 = e->render_value(e->animation);

            e->animation.dist = e->distance[x][y];
            e->animation.angle = e->polar_theta[x][y] + 2 + (e->show2 / 255) * PI;
            ;
            e->animation.z = 5;
            e->animation.scale_x = 0.09;
            e->animation.scale_y = 0.09;
            e->animation.offset_y = -20 * e->move.linear[0];
            e->animation.offset_z = 0;
            e->animation.offset_x = 0;
            e->animation.low_limit = 0;
            e->show4 = e->render_value(e->animation);

            e->show5 = e->screen(e->show4, e->show3);
            e->show6 = e->colordodge(e->show5, e->show3);

            float linear1 = y / 32.f;
            float linear2 = (32 - y) / 32.f;

            e->pixel.red = e->show5 * linear1;
            e->pixel.green = 0;
            e->pixel.blue = e->show6 * linear2;

            e->pixel = e->rgb_sanity_check(e->pixel);

            e->setPixelColorInternal(x, y, e->pixel);
        }
    }
}
inline void SpiralMatrix10(Context &ctx) {
    auto *e = ctx.mEngine;
    e->get_ready();

    e->timings.master_speed = 0.006;
    e->timings.ratio[0] = 0.025;
    e->timings.ratio[1] = 0.027;
    e->timings.ratio[2] = 0.031;
    e->timings.ratio[3] = 0.0053;
    e->timings.ratio[4] = 0.0056;
    e->timings.ratio[5] = 0.0059;

    e->calculate_oscillators(e->timings);

    for (int x = 0; x < e->num_x; x++) {
        for (int y = 0; y < e->num_y; y++) {

            float scale = 0.6;

            e->animation.dist = e->distance[x][y];
            e->animation.angle = e->polar_theta[x][y];
            e->animation.z = 5;
            e->animation.scale_x = 0.09 * scale;
            e->animation.scale_y = 0.09 * scale;
            e->animation.offset_y = -30 * e->move.linear[0];
            e->animation.offset_z = 0;
            e->animation.offset_x = 0;
            e->animation.low_limit = -1;
            e->show1 = e->render_value(e->animation);

            e->animation.dist = e->distance[x][y];
            e->animation.angle = e->polar_theta[x][y];
            e->animation.z = 50;
            e->animation.scale_x = 0.09 * scale;
            e->animation.scale_y = 0.09 * scale;
            e->animation.offset_y = -30 * e->move.linear[1];
            e->animation.offset_z = 0;
            e->animation.offset_x = 0;
            e->animation.low_limit = -1;
            e->show2 = e->render_value(e->animation);

            e->animation.dist = e->distance[x][y];
            e->animation.angle = e->polar_theta[x][y] + 2 + (e->show1 / 255) * PI;
            e->animation.z = 5;
            e->animation.scale_x = 0.09 * scale;
            e->animation.scale_y = 0.09 * scale;
            e->animation.offset_y = -10 * e->move.linear[0];
            e->animation.offset_z = 0;
            e->animation.offset_x = 0;
            e->animation.low_limit = 0;
            e->show3 = e->render_value(e->animation);

            e->animation.dist = e->distance[x][y];
            e->animation.angle = e->polar_theta[x][y] + 2 + (e->show2 / 255) * PI;
            ;
            e->animation.z = 5;
            e->animation.scale_x = 0.09 * scale;
            e->animation.scale_y = 0.09 * scale;
            e->animation.offset_y = -20 * e->move.linear[0];
            e->animation.offset_z = 0;
            e->animation.offset_x = 0;
            e->animation.low_limit = 0;
            e->show4 = e->render_value(e->animation);

            e->show5 = e->screen(e->show4, e->show3);
            e->show6 = e->colordodge(e->show5, e->show3);

            e->pixel.red = (e->show5 + e->show6) / 2;
            e->pixel.green = (e->show5 - 50) + (e->show6 / 16);
            e->pixel.blue = 0;

            e->pixel = e->rgb_sanity_check(e->pixel);

            e->setPixelColorInternal(x, y, e->pixel);
        }
    }
}
inline void Complex_Kaleido(Context &ctx) {
    auto *e = ctx.mEngine;
    e->get_ready();

    e->timings.master_speed = 0.009;
    e->timings.ratio[0] = 0.025;
    e->timings.ratio[1] = 0.027;
    e->timings.ratio[2] = 0.031;
    e->timings.ratio[3] = 0.0053;
    e->timings.ratio[4] = 0.0056;
    e->timings.ratio[5] = 0.0059;

    e->calculate_oscillators(e->timings);

    for (int x = 0; x < e->num_x; x++) {
        for (int y = 0; y < e->num_y; y++) {

            e->animation.dist = e->distance[x][y];
            e->animation.angle = 5 * e->polar_theta[x][y] + 10 * e->move.radial[0] +
                                 e->animation.dist / 2;
            e->animation.z = 5;
            e->animation.scale_x = 0.07;
            e->animation.scale_y = 0.07;
            e->animation.offset_z = 0;
            e->animation.offset_x = -30 * e->move.linear[0];
            e->animation.offset_y = 0;
            e->animation.low_limit = 0;
            e->show1 = e->render_value(e->animation);

            e->animation.dist = e->distance[x][y];
            e->animation.angle = -5 * e->polar_theta[x][y] + 12 * e->move.radial[1] +
                                 e->animation.dist / 2;
            e->animation.z = 500;
            e->animation.scale_x = 0.07;
            e->animation.scale_y = 0.07;
            e->animation.offset_z = 0;
            e->animation.offset_x = -30 * e->move.linear[1];
            e->animation.offset_y = 0;
            e->animation.low_limit = 0;
            e->show2 = e->render_value(e->animation);

            e->animation.dist = e->distance[x][y];
            e->animation.angle = -5 * e->polar_theta[x][y] + 12 * e->move.radial[2] +
                                 e->animation.dist / 2;
            e->animation.z = 500;
            e->animation.scale_x = 0.05;
            e->animation.scale_y = 0.05;
            e->animation.offset_z = 0;
            e->animation.offset_x = -40 * e->move.linear[2];
            e->animation.offset_y = 0;
            e->animation.low_limit = 0;
            e->show3 = e->render_value(e->animation);

            e->animation.dist = e->distance[x][y];
            e->animation.angle = 5 * e->polar_theta[x][y] + 12 * e->move.radial[3] +
                                 e->animation.dist / 2;
            e->animation.z = 500;
            e->animation.scale_x = 0.09;
            e->animation.scale_y = 0.09;
            e->animation.offset_z = 0;
            e->animation.offset_x = -35 * e->move.linear[3];
            e->animation.offset_y = 0;
            e->animation.low_limit = 0;
            e->show4 = e->render_value(e->animation);

            e->show5 = e->screen(e->show4, e->show3);
            e->show6 = e->colordodge(e->show2, e->show3);

            float radius = e->radial_filter_radius;
            float radial = (radius - e->distance[x][y]) / e->distance[x][y];

            e->pixel.red = radial * (e->show1 + e->show2);
            e->pixel.green = 0.3 * radial * e->show6;
            e->pixel.blue = radial * e->show5;

            e->pixel = e->rgb_sanity_check(e->pixel);

            e->setPixelColorInternal(x, y, e->pixel);
        }
    }
}
inline void Complex_Kaleido_2(Context &ctx) {
    auto *e = ctx.mEngine;
    e->get_ready();

    e->timings.master_speed = 0.009;
    e->timings.ratio[0] = 0.025;
    e->timings.ratio[1] = 0.027;
    e->timings.ratio[2] = 0.031;
    e->timings.ratio[3] = 0.0053;
    e->timings.ratio[4] = 0.0056;
    e->timings.ratio[5] = 0.0059;

    e->calculate_oscillators(e->timings);

    float size = 0.5;

    for (int x = 0; x < e->num_x; x++) {
        for (int y = 0; y < e->num_y; y++) {

            e->animation.dist = e->distance[x][y];
            e->animation.angle = 5 * e->polar_theta[x][y] + 10 * e->move.radial[0] +
                                 e->animation.dist / 2;
            e->animation.z = 5;
            e->animation.scale_x = 0.07 * size;
            e->animation.scale_y = 0.07 * size;
            e->animation.offset_z = 0;
            e->animation.offset_x = -30 * e->move.linear[0];
            e->animation.offset_y = 0;
            e->animation.low_limit = 0;
            e->show1 = e->render_value(e->animation);

            e->animation.dist = e->distance[x][y];
            e->animation.angle = -5 * e->polar_theta[x][y] + 12 * e->move.radial[1] +
                                 e->animation.dist / 2;
            e->animation.z = 500;
            e->animation.scale_x = 0.07 * size;
            e->animation.scale_y = 0.07 * size;
            e->animation.offset_z = 0;
            e->animation.offset_x = -30 * e->move.linear[1];
            e->animation.offset_y = 0;
            e->animation.low_limit = 0;
            e->show2 = e->render_value(e->animation);

            e->animation.dist = e->distance[x][y];
            e->animation.angle = -5 * e->polar_theta[x][y] + 12 * e->move.radial[2] +
                                 e->animation.dist / 2;
            e->animation.z = 500;
            e->animation.scale_x = 0.05 * size;
            e->animation.scale_y = 0.05 * size;
            e->animation.offset_z = 0;
            e->animation.offset_x = -40 * e->move.linear[2];
            e->animation.offset_y = 0;
            e->animation.low_limit = 0;
            e->show3 = e->render_value(e->animation);

            e->animation.dist = e->distance[x][y];
            e->animation.angle = 5 * e->polar_theta[x][y] + 12 * e->move.radial[3] +
                                 e->animation.dist / 2;
            e->animation.z = 500;
            e->animation.scale_x = 0.09 * size;
            e->animation.scale_y = 0.09 * size;
            e->animation.offset_z = 0;
            e->animation.offset_x = -35 * e->move.linear[3];
            e->animation.offset_y = 0;
            e->animation.low_limit = 0;
            e->show4 = e->render_value(e->animation);

            e->show5 = e->screen(e->show4, e->show3);
            e->show6 = e->colordodge(e->show2, e->show3);

            float radius = e->radial_filter_radius;
            float radial = (radius - e->distance[x][y]) / e->distance[x][y];

            e->pixel.red = radial * (e->show1 + e->show2);
            e->pixel.green = 0.3 * radial * e->show6;
            e->pixel.blue = radial * e->show5;

            e->pixel = e->rgb_sanity_check(e->pixel);

            e->setPixelColorInternal(x, y, e->pixel);
        }
    }
}
inline void Complex_Kaleido_3(Context &ctx) {
    auto *e = ctx.mEngine;
    e->get_ready();

    e->timings.master_speed = 0.001;
    e->timings.ratio[0] = 0.025;
    e->timings.ratio[1] = 0.027;
    e->timings.ratio[2] = 0.031;
    e->timings.ratio[3] = 0.033;
    e->timings.ratio[4] = 0.037;
    e->timings.ratio[5] = 0.038;
    e->timings.ratio[5] = 0.041;

    e->calculate_oscillators(e->timings);

    float size = 0.4 + e->move.directional[0] * 0.1;

    float q = 2;

    for (int x = 0; x < e->num_x; x++) {
        for (int y = 0; y < e->num_y; y++) {

            e->animation.dist = e->distance[x][y];
            e->animation.angle =
                5 * e->polar_theta[x][y] + 10 * e->move.radial[0] +
                e->animation.dist / (((e->move.directional[0] + 3) * 2)) +
                e->move.noise_angle[0] * q;
            e->animation.z = 5;
            e->animation.scale_x = 0.08 * size * (e->move.directional[0] + 1.5);
            e->animation.scale_y = 0.07 * size;
            e->animation.offset_z = -10 * e->move.linear[0];
            e->animation.offset_x = -30 * e->move.linear[0];
            e->animation.offset_y = 0;
            e->animation.low_limit = 0;
            e->show1 = e->render_value(e->animation);

            e->animation.dist = e->distance[x][y];
            e->animation.angle =
                -5 * e->polar_theta[x][y] + 10 * e->move.radial[1] +
                e->animation.dist / (((e->move.directional[1] + 3) * 2)) +
                e->move.noise_angle[1] * q;
            e->animation.z = 500;
            e->animation.scale_x = 0.07 * size * (e->move.directional[1] + 1.1);
            e->animation.scale_y = 0.07 * size * (e->move.directional[2] + 1.3);
            ;
            e->animation.offset_z = -12 * e->move.linear[1];
            ;
            e->animation.offset_x = -(e->num_x - 1) * e->move.linear[1];
            e->animation.offset_y = 0;
            e->animation.low_limit = 0;
            e->show2 = e->render_value(e->animation);

            e->animation.dist = e->distance[x][y];
            e->animation.angle =
                -5 * e->polar_theta[x][y] + 12 * e->move.radial[2] +
                e->animation.dist / (((e->move.directional[3] + 3) * 2)) +
                e->move.noise_angle[2] * q;
            e->animation.z = 500;
            e->animation.scale_x = 0.05 * size * (e->move.directional[3] + 1.5);
            ;
            e->animation.scale_y = 0.05 * size * (e->move.directional[4] + 1.5);
            ;
            e->animation.offset_z = -12 * e->move.linear[3];
            e->animation.offset_x = -40 * e->move.linear[3];
            e->animation.offset_y = 0;
            e->animation.low_limit = 0;
            e->show3 = e->render_value(e->animation);

            e->animation.dist = e->distance[x][y];
            e->animation.angle =
                5 * e->polar_theta[x][y] + 12 * e->move.radial[3] +
                e->animation.dist / (((e->move.directional[5] + 3) * 2)) +
                e->move.noise_angle[3] * q;
            e->animation.z = 500;
            e->animation.scale_x = 0.09 * size * (e->move.directional[5] + 1.5);
            ;
            ;
            e->animation.scale_y = 0.09 * size * (e->move.directional[6] + 1.5);
            ;
            ;
            e->animation.offset_z = 0;
            e->animation.offset_x = -35 * e->move.linear[3];
            e->animation.offset_y = 0;
            e->animation.low_limit = 0;
            e->show4 = e->render_value(e->animation);

            e->show5 = e->screen(e->show4, e->show3) - e->show2;
            e->show6 = e->colordodge(e->show4, e->show1);

            e->show7 = e->multiply(e->show1, e->show2);

            float linear1 = y / 32.f;

            float radius = e->radial_filter_radius;
            float radial = (radius - e->distance[x][y]) / e->distance[x][y];

            e->show7 = e->multiply(e->show1, e->show2) * linear1 * 2;
            e->show8 = e->subtract(e->show7, e->show5);

            e->pixel.green = 0.2 * e->show8;
            e->pixel.blue = e->show5 * radial;
            e->pixel.red = (1 * e->show1 + 1 * e->show2) - e->show7 / 2;

            e->pixel = e->rgb_sanity_check(e->pixel);

            e->setPixelColorInternal(x, y, e->pixel);
        }
    }
}
inline void Complex_Kaleido_4(Context &ctx) {
    auto *e = ctx.mEngine;
    e->get_ready();

    e->timings.master_speed = 0.01;
    e->timings.ratio[0] = 0.025;
    e->timings.ratio[1] = 0.027;
    e->timings.ratio[2] = 0.031;
    e->timings.ratio[3] = 0.033;
    e->timings.ratio[4] = 0.037;
    e->timings.ratio[5] = 0.038;
    e->timings.ratio[6] = 0.041;

    e->calculate_oscillators(e->timings);

    float size = 0.6;

    float q = 1;

    for (int x = 0; x < e->num_x; x++) {
        for (int y = 0; y < e->num_y; y++) {

            float s = 1 + e->move.directional[6] * 0.3;

            e->animation.dist = e->distance[x][y] * s;
            e->animation.angle =
                5 * e->polar_theta[x][y] + 1 * e->move.radial[0] -
                e->animation.dist / (3 + e->move.directional[0] * 0.5);
            e->animation.z = 5;
            e->animation.scale_x = 0.08 * size + (e->move.directional[0] * 0.01);
            e->animation.scale_y = 0.07 * size + (e->move.directional[1] * 0.01);
            e->animation.offset_z = -10 * e->move.linear[0];
            e->animation.offset_x = 0;
            e->animation.offset_y = 0;
            e->animation.low_limit = 0;
            e->show1 = e->render_value(e->animation);

            e->animation.dist = e->distance[x][y] * s;
            e->animation.angle =
                5 * e->polar_theta[x][y] + 1 * e->move.radial[1] +
                e->animation.dist / (3 + e->move.directional[1] * 0.5);
            e->animation.z = 50;
            e->animation.scale_x = 0.08 * size + (e->move.directional[1] * 0.01);
            e->animation.scale_y = 0.07 * size + (e->move.directional[2] * 0.01);
            e->animation.offset_z = -10 * e->move.linear[1];
            e->animation.offset_x = 0;
            e->animation.offset_y = 0;
            e->animation.low_limit = 0;
            e->show2 = e->render_value(e->animation);

            e->animation.dist = e->distance[x][y];
            e->animation.angle = 1;
            e->animation.z = 500;
            e->animation.scale_x = 0.2 * size;
            e->animation.scale_y = 0.2 * size;
            e->animation.offset_z = 0;
            e->animation.offset_y = +7 * e->move.linear[3] + e->move.noise_angle[3];
            e->animation.offset_x = 0;
            e->animation.low_limit = 0;
            e->show3 = e->render_value(e->animation);

            e->animation.dist = e->distance[x][y];
            e->animation.angle =
                5 * e->polar_theta[x][y] + 12 * e->move.radial[3] +
                e->animation.dist / (((e->move.directional[5] + 3) * 2)) +
                e->move.noise_angle[3] * q;
            e->animation.z = 500;
            e->animation.scale_x = 0.09 * size * (e->move.directional[5] + 1.5);
            ;
            ;
            e->animation.scale_y = 0.09 * size * (e->move.directional[6] + 1.5);
            ;
            ;
            e->animation.offset_z = 0;
            e->animation.offset_x = -35 * e->move.linear[3];
            e->animation.offset_y = 0;
            e->animation.low_limit = 0;
            e->show4 = e->render_value(e->animation);

            float radius = e->radial_filter_radius;
            float radial = (radius - e->distance[x][y]) / e->distance[x][y];

            e->show5 = ((e->show1 + e->show2)) - e->show3;
            if (e->show5 > 255)
                e->show5 = 255;
            if (e->show5 < 0)
                e->show5 = 0;

            e->show6 = e->colordodge(e->show1, e->show2);

            e->pixel.red = e->show5 * radial;
            e->pixel.blue = (64 - e->show5 - e->show3) * radial;
            e->pixel.green = 0.5 * (e->show6);

            e->pixel = e->rgb_sanity_check(e->pixel);

            e->setPixelColorInternal(x, y, e->pixel);
        }
    }
}
inline void Complex_Kaleido_5(Context &ctx) {
    auto *e = ctx.mEngine;
    e->get_ready();

    e->timings.master_speed = 0.01;
    e->timings.ratio[0] = 0.025;
    e->timings.ratio[1] = 0.027;
    e->timings.ratio[2] = 0.031;
    e->timings.ratio[3] = 0.033;
    e->timings.ratio[4] = 0.037;
    e->timings.ratio[5] = 0.0038;
    e->timings.ratio[6] = 0.041;

    e->calculate_oscillators(e->timings);

    float size = 0.6;

    for (int x = 0; x < e->num_x; x++) {
        for (int y = 0; y < e->num_y; y++) {

            float s = 1 + e->move.directional[6] * 0.8;

            e->animation.dist = e->distance[x][y] * s;
            e->animation.angle = 10 * e->move.radial[6] +
                                 50 * e->move.directional[5] * e->polar_theta[x][y] -
                                 e->animation.dist / 3;
            e->animation.z = 5;
            e->animation.scale_x = 0.08 * size;
            e->animation.scale_y = 0.07 * size;
            e->animation.offset_z = -10 * e->move.linear[0];
            e->animation.offset_x = 0;
            e->animation.offset_y = 0;
            e->animation.low_limit = -0.5;
            e->show1 = e->render_value(e->animation);

            float radius = e->radial_filter_radius;
            float radial = (radius - e->distance[x][y]) / e->distance[x][y];

            e->pixel.red = e->show1 * radial;
            e->pixel.green = 0;
            e->pixel.blue = 0;

            e->pixel = e->rgb_sanity_check(e->pixel);

            e->setPixelColorInternal(x, y, e->pixel);
        }
    }
}
inline void Complex_Kaleido_6(Context &ctx) {
    auto *e = ctx.mEngine;
    e->get_ready();

    e->timings.master_speed = 0.01;
    e->timings.ratio[0] = 0.025;
    e->timings.ratio[1] = 0.027;
    e->timings.ratio[2] = 0.031;
    e->timings.ratio[3] = 0.033;
    e->timings.ratio[4] = 0.037;
    e->timings.ratio[5] = 0.0038;
    e->timings.ratio[6] = 0.041;

    e->calculate_oscillators(e->timings);

    for (int x = 0; x < e->num_x; x++) {
        for (int y = 0; y < e->num_y; y++) {

            e->animation.dist = e->distance[x][y];
            e->animation.angle = 16 * e->polar_theta[x][y] + 16 * e->move.radial[0];
            e->animation.z = 5;
            e->animation.scale_x = 0.06;
            e->animation.scale_y = 0.06;
            e->animation.offset_z = -10 * e->move.linear[0];
            e->animation.offset_y = 10 * e->move.noise_angle[0];
            e->animation.offset_x = 10 * e->move.noise_angle[4];
            e->animation.low_limit = 0;
            e->show1 = e->render_value(e->animation);

            e->animation.dist = e->distance[x][y];
            e->animation.angle = 16 * e->polar_theta[x][y] + 16 * e->move.radial[1];
            e->animation.z = 500;
            e->animation.scale_x = 0.06;
            e->animation.scale_y = 0.06;
            e->animation.offset_z = -10 * e->move.linear[1];
            e->animation.offset_y = 10 * e->move.noise_angle[1];
            e->animation.offset_x = 10 * e->move.noise_angle[3];
            e->animation.low_limit = 0;
            e->show2 = e->render_value(e->animation);

            e->pixel.red = e->show1;
            e->pixel.green = 0;
            e->pixel.blue = e->show2;

            e->pixel = e->rgb_sanity_check(e->pixel);

            e->setPixelColorInternal(x, y, e->pixel);
        }
    }
}
inline void Water(Context &ctx) {
    auto *e = ctx.mEngine;
    e->get_ready();

    e->timings.master_speed = 0.037;
    e->timings.ratio[0] = 0.025;
    e->timings.ratio[1] = 0.027;
    e->timings.ratio[2] = 0.031;
    e->timings.ratio[3] = 0.033;
    e->timings.ratio[4] = 0.037;
    e->timings.ratio[5] = 0.1;
    e->timings.ratio[6] = 0.41;

    e->calculate_oscillators(e->timings);

    for (int x = 0; x < e->num_x; x++) {
        for (int y = 0; y < e->num_y; y++) {

            e->animation.dist =
                e->distance[x][y] +
                4 * fl::sinf(e->move.directional[5] * PI + (float)x / 2) +
                4 * fl::cosf(e->move.directional[6] * PI + float(y) / 2);
            e->animation.angle = 1 * e->polar_theta[x][y];
            e->animation.z = 5;
            e->animation.scale_x = 0.06;
            e->animation.scale_y = 0.06;
            e->animation.offset_z = -10 * e->move.linear[0];
            e->animation.offset_y = 10;
            e->animation.offset_x = 10;
            e->animation.low_limit = 0;
            e->show1 = e->render_value(e->animation);

            e->animation.dist = (10 + e->move.directional[0]) *
                                 fl::sinf(-e->move.radial[5] + e->move.radial[0] +
                                      (e->distance[x][y] / (3)));
            e->animation.angle = 1 * e->polar_theta[x][y];
            e->animation.z = 5;
            e->animation.scale_x = 0.1;
            e->animation.scale_y = 0.1;
            e->animation.offset_z = -10;
            e->animation.offset_y = 20 * e->move.linear[0];
            e->animation.offset_x = 10;
            e->animation.low_limit = 0;
            e->show2 = e->render_value(e->animation);

            e->animation.dist = (10 + e->move.directional[1]) *
                                 fl::sinf(-e->move.radial[5] + e->move.radial[1] +
                                      (e->distance[x][y] / (3)));
            e->animation.angle = 1 * e->polar_theta[x][y];
            e->animation.z = 500;
            e->animation.scale_x = 0.1;
            e->animation.scale_y = 0.1;
            e->animation.offset_z = -10;
            e->animation.offset_y = 20 * e->move.linear[1];
            e->animation.offset_x = 10;
            e->animation.low_limit = 0;
            e->show3 = e->render_value(e->animation);

            e->animation.dist = (10 + e->move.directional[2]) *
                                 fl::sinf(-e->move.radial[5] + e->move.radial[2] +
                                      (e->distance[x][y] / (3)));
            e->animation.angle = 1 * e->polar_theta[x][y];
            e->animation.z = 500;
            e->animation.scale_x = 0.1;
            e->animation.scale_y = 0.1;
            e->animation.offset_z = -10;
            e->animation.offset_y = 20 * e->move.linear[2];
            e->animation.offset_x = 10;
            e->animation.low_limit = 0;
            e->show4 = e->render_value(e->animation);

            e->pixel.blue = (0.7 * e->show2 + 0.6 * e->show3 + 0.5 * e->show4);
            e->pixel.red = e->pixel.blue - 40;
            e->pixel.green = 0;

            e->pixel = e->rgb_sanity_check(e->pixel);

            e->setPixelColorInternal(x, y, e->pixel);
        }
    }
}
inline void Parametric_Water(Context &ctx) {
    auto *e = ctx.mEngine;
    e->get_ready();

    e->timings.master_speed = 0.003;
    e->timings.ratio[0] = 0.025;
    e->timings.ratio[1] = 0.027;
    e->timings.ratio[2] = 0.029;
    e->timings.ratio[3] = 0.033;
    e->timings.ratio[4] = 0.037;
    e->timings.ratio[5] = 0.15;
    e->timings.ratio[6] = 0.41;

    e->calculate_oscillators(e->timings);

    for (int x = 0; x < e->num_x; x++) {
        for (int y = 0; y < e->num_y; y++) {

            float s = 4;
            float f = 10 + 2 * e->move.directional[0];

            e->animation.dist = (f + e->move.directional[0]) *
                                 fl::sinf(-e->move.radial[5] + e->move.radial[0] +
                                      (e->distance[x][y] / (s)));
            e->animation.angle = 1 * e->polar_theta[x][y];
            e->animation.z = 5;
            e->animation.scale_x = 0.1;
            e->animation.scale_y = 0.1;
            e->animation.offset_z = -10;
            e->animation.offset_y = 20 * e->move.linear[0];
            e->animation.offset_x = 10;
            e->animation.low_limit = 0;
            e->show2 = e->render_value(e->animation);

            e->animation.dist = (f + e->move.directional[1]) *
                                 fl::sinf(-e->move.radial[5] + e->move.radial[1] +
                                      (e->distance[x][y] / (s)));
            e->animation.angle = 1 * e->polar_theta[x][y];
            e->animation.z = 500;
            e->animation.scale_x = 0.1;
            e->animation.scale_y = 0.1;
            e->animation.offset_z = -10;
            e->animation.offset_y = 20 * e->move.linear[1];
            e->animation.offset_x = 10;
            e->animation.low_limit = 0;
            e->show3 = e->render_value(e->animation);

            e->animation.dist = (f + e->move.directional[2]) *
                                 fl::sinf(-e->move.radial[5] + e->move.radial[2] +
                                      (e->distance[x][y] / (s)));
            e->animation.angle = 1 * e->polar_theta[x][y];
            e->animation.z = 5000;
            e->animation.scale_x = 0.1;
            e->animation.scale_y = 0.1;
            e->animation.offset_z = -10;
            e->animation.offset_y = 20 * e->move.linear[2];
            e->animation.offset_x = 10;
            e->animation.low_limit = 0;
            e->show4 = e->render_value(e->animation);

            e->animation.dist = (f + e->move.directional[3]) *
                                 fl::sinf(-e->move.radial[5] + e->move.radial[3] +
                                      (e->distance[x][y] / (s)));
            e->animation.angle = 1 * e->polar_theta[x][y];
            e->animation.z = 2000;
            e->animation.scale_x = 0.1;
            e->animation.scale_y = 0.1;
            e->animation.offset_z = -10;
            e->animation.offset_y = 20 * e->move.linear[3];
            e->animation.offset_x = 10;
            e->animation.low_limit = 0;
            e->show5 = e->render_value(e->animation);

            e->show6 = e->screen(e->show4, e->show5);
            e->show7 = e->screen(e->show2, e->show3);

            float radius = 40;
            float radial = (radius - e->distance[x][y]) / radius;

            e->pixel.red = e->pixel.blue - 40;
            e->pixel.green = 0;
            e->pixel.blue = (0.3 * e->show6 + 0.7 * e->show7) * radial;

            e->pixel = e->rgb_sanity_check(e->pixel);

            e->setPixelColorInternal(x, y, e->pixel);
        }
    }
}
inline void Module_Experiment1(Context &ctx) {
    auto *e = ctx.mEngine;
    e->get_ready();

    e->timings.master_speed = 0.03;
    e->timings.ratio[0] = 0.0025;
    e->timings.ratio[1] = 0.0027;
    e->timings.ratio[2] = 0.029;
    e->timings.ratio[3] = 0.033;

    e->calculate_oscillators(e->timings);

    for (int x = 0; x < e->num_x; x++) {
        for (int y = 0; y < e->num_y; y++) {

            e->animation.dist = e->distance[x][y] + 20 * e->move.directional[0];
            e->animation.angle = e->move.noise_angle[0] + e->move.noise_angle[1] +
                                 e->polar_theta[x][y];
            e->animation.z = 5;
            e->animation.scale_x = 0.1;
            e->animation.scale_y = 0.1;
            e->animation.offset_z = -10;
            e->animation.offset_y = 20 * e->move.linear[2];
            e->animation.offset_x = 10;
            e->animation.low_limit = 0;
            e->show1 = e->render_value(e->animation);

            e->pixel.red = 0;
            e->pixel.green = 0;
            e->pixel.blue = e->show1;

            e->pixel = e->rgb_sanity_check(e->pixel);

            e->setPixelColorInternal(x, y, e->pixel);
        }
    }
}
inline void Module_Experiment2(Context &ctx) {
    auto *e = ctx.mEngine;
    e->get_ready();

    e->timings.master_speed = 0.02;
    e->timings.ratio[0] = 0.0025;
    e->timings.ratio[1] = 0.0027;
    e->timings.ratio[2] = 0.029;
    e->timings.ratio[3] = 0.033;

    e->calculate_oscillators(e->timings);

    for (int x = 0; x < e->num_x; x++) {
        for (int y = 0; y < e->num_y; y++) {

            e->animation.dist =
                e->distance[x][y] - (16 + e->move.directional[0] * 16);
            e->animation.angle = e->move.noise_angle[0] + e->move.noise_angle[1] +
                                 e->polar_theta[x][y];
            e->animation.z = 5;
            e->animation.scale_x = 0.1;
            e->animation.scale_y = 0.1;
            e->animation.offset_z = -10;
            e->animation.offset_y = 20 * e->move.linear[2];
            e->animation.offset_x = 10;
            e->animation.low_limit = 0;
            e->show1 = e->render_value(e->animation);

            e->pixel.red = e->show1;
            e->pixel.green = e->show1 - 80;
            e->pixel.blue = e->show1 - 150;

            e->pixel = e->rgb_sanity_check(e->pixel);

            e->setPixelColorInternal(x, y, e->pixel);
        }
    }
}
inline void Module_Experiment3(Context &ctx) {
    auto *e = ctx.mEngine;
    e->get_ready();

    e->timings.master_speed = 0.01;
    e->timings.ratio[0] = 0.0025;
    e->timings.ratio[1] = 0.0027;
    e->timings.ratio[2] = 0.029;
    e->timings.ratio[3] = 0.033;

    e->calculate_oscillators(e->timings);

    for (int x = 0; x < e->num_x; x++) {
        for (int y = 0; y < e->num_y; y++) {

            e->animation.dist =
                e->distance[x][y] - (12 + e->move.directional[3] * 4);
            e->animation.angle = e->move.noise_angle[0] + e->move.noise_angle[1] +
                                 e->polar_theta[x][y];
            e->animation.z = 5;
            e->animation.scale_x = 0.1;
            e->animation.scale_y = 0.1;
            e->animation.offset_z = -10;
            e->animation.offset_y = 20 * e->move.linear[2];
            e->animation.offset_x = 10;
            e->animation.low_limit = 0;
            e->show1 = e->render_value(e->animation);

            e->pixel.red = e->show1;
            e->pixel.green = e->show1 - 80;
            e->pixel.blue = e->show1 - 150;

            e->pixel = e->rgb_sanity_check(e->pixel);

            e->setPixelColorInternal(x, y, e->pixel);
        }
    }
}
inline void Zoom2(Context &ctx) {
    auto *e = ctx.mEngine;
    e->get_ready();

    e->run_default_oscillators();
    e->timings.master_speed = 0.003;
    e->calculate_oscillators(e->timings);

    for (int x = 0; x < e->num_x; x++) {
        for (int y = 0; y < e->num_y; y++) {
            e->animation.dist = (e->distance[x][y] * e->distance[x][y]) / 2;
            e->animation.angle = e->polar_theta[x][y];

            e->animation.scale_x = 0.005;
            e->animation.scale_y = 0.005;

            e->animation.offset_y = -10 * e->move.linear[0];
            e->animation.offset_x = 0;
            e->animation.offset_z = 0.1 * e->move.linear[0];

            e->animation.z = 0;
            e->animation.low_limit = 0;
            float show1 = e->render_value(e->animation);

            e->pixel.red = show1;
            e->pixel.green = 0;
            e->pixel.blue = 40 - show1;

            e->pixel = e->rgb_sanity_check(e->pixel);
            e->setPixelColorInternal(x, y, e->pixel);
        }
    }
}
inline void Module_Experiment4(Context &ctx) {
    auto *e = ctx.mEngine;
    e->get_ready();

    e->timings.master_speed = 0.031;
    e->timings.ratio[0] = 0.0025;
    e->timings.ratio[1] = 0.0027;
    e->timings.ratio[2] = 0.029;
    e->timings.ratio[3] = 0.033;
    e->timings.ratio[4] = 0.036;

    e->calculate_oscillators(e->timings);

    for (int x = 0; x < e->num_x; x++) {
        for (int y = 0; y < e->num_y; y++) {

            float s = 0.8;

            e->animation.dist = (e->distance[x][y] * e->distance[x][y]) * 0.7;
            e->animation.angle = e->polar_theta[x][y];
            e->animation.z = 5;
            e->animation.scale_x = 0.004 * s;
            e->animation.scale_y = 0.003 * s;
            e->animation.offset_z = 0.1 * e->move.linear[2];
            e->animation.offset_y = -20 * e->move.linear[2];
            e->animation.offset_x = 10;
            e->animation.low_limit = 0;
            e->show1 = e->render_value(e->animation);

            e->animation.dist = (e->distance[x][y] * e->distance[x][y]) * 0.8;
            e->animation.angle = e->polar_theta[x][y];
            e->animation.z = 50;
            e->animation.scale_x = 0.004 * s;
            e->animation.scale_y = 0.003 * s;
            e->animation.offset_z = 0.1 * e->move.linear[3];
            e->animation.offset_y = -20 * e->move.linear[3];
            e->animation.offset_x = 100;
            e->animation.low_limit = 0;
            e->show2 = e->render_value(e->animation);

            e->animation.dist = (e->distance[x][y] * e->distance[x][y]) * 0.9;
            e->animation.angle = e->polar_theta[x][y];
            e->animation.z = 5000;
            e->animation.scale_x = 0.004 * s;
            e->animation.scale_y = 0.003 * s;
            e->animation.offset_z = 0.1 * e->move.linear[4];
            e->animation.offset_y = -20 * e->move.linear[4];
            e->animation.offset_x = 1000;
            e->animation.low_limit = 0;
            e->show3 = e->render_value(e->animation);

            e->pixel.red = e->show1 - e->show2 - e->show3;
            e->pixel.blue = e->show2 - e->show1 - e->show3;
            e->pixel.green = e->show3 - e->show1 - e->show2;

            e->pixel = e->rgb_sanity_check(e->pixel);

            e->setPixelColorInternal(x, y, e->pixel);
        }
    }
}
inline void Module_Experiment5(Context &ctx) {
    auto *e = ctx.mEngine;
    e->get_ready();

    e->timings.master_speed = 0.031;
    e->timings.ratio[0] = 0.0025;
    e->timings.ratio[1] = 0.0027;
    e->timings.ratio[2] = 0.029;
    e->timings.ratio[3] = 0.33;
    e->timings.ratio[4] = 0.036;

    e->calculate_oscillators(e->timings);

    for (int x = 0; x < e->num_x; x++) {
        for (int y = 0; y < e->num_y; y++) {

            float s = 1.5;

            e->animation.dist = e->distance[x][y] +
                                 fl::sinf(0.5 * e->distance[x][y] - e->move.radial[3]);
            e->animation.angle = e->polar_theta[x][y];
            e->animation.z = 5;
            e->animation.scale_x = 0.1 * s;
            e->animation.scale_y = 0.1 * s;
            e->animation.offset_z = 0.1 * e->move.linear[0];
            e->animation.offset_y = -20 * e->move.linear[0];
            e->animation.offset_x = 10;
            e->animation.low_limit = 0;
            e->show1 = e->render_value(e->animation);

            e->pixel.red = e->show1;
            e->pixel.green = 0;
            e->pixel.blue = 0;

            e->pixel = e->rgb_sanity_check(e->pixel);

            e->setPixelColorInternal(x, y, e->pixel);
        }
    }
}
inline void Module_Experiment6(Context &ctx) {
    auto *e = ctx.mEngine;
    e->get_ready();

    e->timings.master_speed = 0.01;

    float w = 0.7;

    e->timings.ratio[0] = 0.0025;
    e->timings.ratio[1] = 0.0027;
    e->timings.ratio[2] = 0.029;
    e->timings.ratio[3] = 0.33 * w;
    e->timings.ratio[4] = 0.36 * w;

    e->calculate_oscillators(e->timings);

    for (int x = 0; x < e->num_x; x++) {
        for (int y = 0; y < e->num_y; y++) {

            float s = 0.8;

            e->animation.dist = e->distance[x][y] +
                                 fl::sinf(0.25 * e->distance[x][y] - e->move.radial[3]);
            e->animation.angle = e->polar_theta[x][y];
            e->animation.z = 5;
            e->animation.scale_x = 0.1 * s;
            e->animation.scale_y = 0.1 * s;
            e->animation.offset_z = 0.1 * e->move.linear[0];
            e->animation.offset_y = -20 * e->move.linear[0];
            e->animation.offset_x = 10;
            e->animation.low_limit = 0;
            e->show1 = e->render_value(e->animation);

            e->animation.dist = e->distance[x][y] +
                                 fl::sinf(0.24 * e->distance[x][y] - e->move.radial[4]);
            e->animation.angle = e->polar_theta[x][y];
            e->animation.z = 10;
            e->animation.scale_x = 0.1 * s;
            e->animation.scale_y = 0.1 * s;
            e->animation.offset_z = 0.1 * e->move.linear[1];
            e->animation.offset_y = -20 * e->move.linear[1];
            e->animation.offset_x = 10;
            e->animation.low_limit = 0;
            e->show2 = e->render_value(e->animation);

            e->pixel.red = (e->show1 + e->show2);
            e->pixel.green = ((e->show1 + e->show2) * 0.6) - 30;
            e->pixel.blue = 0;

            e->pixel = e->rgb_sanity_check(e->pixel);

            e->setPixelColorInternal(x, y, e->pixel);
        }
    }
}
inline void Module_Experiment7(Context &ctx) {
    auto *e = ctx.mEngine;
    e->get_ready();

    e->timings.master_speed = 0.005;

    float w = 0.3;

    e->timings.ratio[0] = 0.01;
    e->timings.ratio[1] = 0.011;
    e->timings.ratio[2] = 0.029;
    e->timings.ratio[3] = 0.33 * w;
    e->timings.ratio[4] = 0.36 * w;

    e->calculate_oscillators(e->timings);

    for (int x = 0; x < e->num_x; x++) {
        for (int y = 0; y < e->num_y; y++) {

            float s = 0.7;

            e->animation.dist =
                2 + e->distance[x][y] +
                2 * fl::sinf(0.25 * e->distance[x][y] - e->move.radial[3]);
            e->animation.angle = e->polar_theta[x][y];
            e->animation.z = 5;
            e->animation.scale_x = 0.1 * s;
            e->animation.scale_y = 0.1 * s;
            e->animation.offset_z = 10 * e->move.linear[0];
            e->animation.offset_y = -20 * e->move.linear[0];
            e->animation.offset_x = 10;
            e->animation.low_limit = 0;
            e->show1 = e->render_value(e->animation);

            e->animation.dist =
                2 + e->distance[x][y] +
                2 * fl::sinf(0.24 * e->distance[x][y] - e->move.radial[4]);
            e->animation.angle = e->polar_theta[x][y];
            e->animation.z = 10;
            e->animation.scale_x = 0.1 * s;
            e->animation.scale_y = 0.1 * s;
            e->animation.offset_z = 0.1 * e->move.linear[1];
            e->animation.offset_y = -20 * e->move.linear[1];
            e->animation.offset_x = 10;
            e->animation.low_limit = 0;
            e->show2 = e->render_value(e->animation);

            e->pixel.red = (e->show1 + e->show2);
            e->pixel.green = ((e->show1 + e->show2) * 0.6) - 50;
            e->pixel.blue = 0;

            e->pixel = e->rgb_sanity_check(e->pixel);

            e->setPixelColorInternal(x, y, e->pixel);
        }
    }
}
inline void Module_Experiment8(Context &ctx) {
    auto *e = ctx.mEngine;
    e->get_ready();

    e->timings.master_speed = 0.01;

    float w = 0.3;

    e->timings.ratio[0] = 0.01;
    e->timings.ratio[1] = 0.011;
    e->timings.ratio[2] = 0.013;
    e->timings.ratio[3] = 0.33 * w;
    e->timings.ratio[4] = 0.36 * w;
    e->timings.ratio[5] = 0.38 * w;
    e->timings.ratio[6] = 0.0003;

    e->timings.offset[0] = 0;
    e->timings.offset[1] = 100;
    e->timings.offset[2] = 200;
    e->timings.offset[3] = 300;
    e->timings.offset[4] = 400;
    e->timings.offset[5] = 500;
    e->timings.offset[6] = 600;

    e->calculate_oscillators(e->timings);

    for (int x = 0; x < e->num_x; x++) {
        for (int y = 0; y < e->num_y; y++) {

            float s = 0.4;
            float r = 1.5;

            e->animation.dist =
                3 + e->distance[x][y] +
                3 * fl::sinf(0.25 * e->distance[x][y] - e->move.radial[3]);
            e->animation.angle = e->polar_theta[x][y] + e->move.noise_angle[0] +
                                 e->move.noise_angle[6];
            e->animation.z = 5;
            e->animation.scale_x = 0.1 * s;
            e->animation.scale_y = 0.1 * s;
            e->animation.offset_z = 10 * e->move.linear[0];
            e->animation.offset_y = -5 * r * e->move.linear[0];
            e->animation.offset_x = 10;
            e->animation.low_limit = 0;
            e->show1 = e->render_value(e->animation);

            e->animation.dist =
                4 + e->distance[x][y] +
                4 * fl::sinf(0.24 * e->distance[x][y] - e->move.radial[4]);
            e->animation.angle = e->polar_theta[x][y] + e->move.noise_angle[1] +
                                 e->move.noise_angle[6];
            e->animation.z = 5;
            e->animation.scale_x = 0.1 * s;
            e->animation.scale_y = 0.1 * s;
            e->animation.offset_z = 0.1 * e->move.linear[1];
            e->animation.offset_y = -5 * r * e->move.linear[1];
            e->animation.offset_x = 100;
            e->animation.low_limit = 0;
            e->show2 = e->render_value(e->animation);

            e->animation.dist =
                5 + e->distance[x][y] +
                5 * fl::sinf(0.23 * e->distance[x][y] - e->move.radial[5]);
            e->animation.angle = e->polar_theta[x][y] + e->move.noise_angle[2] +
                                 e->move.noise_angle[6];
            e->animation.z = 5;
            e->animation.scale_x = 0.1 * s;
            e->animation.scale_y = 0.1 * s;
            e->animation.offset_z = 0.1 * e->move.linear[2];
            e->animation.offset_y = -5 * r * e->move.linear[2];
            e->animation.offset_x = 1000;
            e->animation.low_limit = 0;
            e->show3 = e->render_value(e->animation);

            e->show4 = e->colordodge(e->show1, e->show2);

            float rad = fl::sinf(PI / 2 +
                             e->distance[x][y] / 14);

            e->pixel.red = rad * ((e->show1 + e->show2) + e->show3);
            e->pixel.green = (((e->show2 + e->show3) * 0.8) - 90) * rad;
            e->pixel.blue = e->show4 * 0.2;

            e->pixel = e->rgb_sanity_check(e->pixel);

            e->setPixelColorInternal(x, y, e->pixel);
        }
    }
}
inline void Module_Experiment9(Context &ctx) {
    auto *e = ctx.mEngine;
    e->get_ready();

    e->timings.master_speed = 0.03;

    float w = 0.3;

    e->timings.ratio[0] = 0.1;
    e->timings.ratio[1] = 0.011;
    e->timings.ratio[2] = 0.013;
    e->timings.ratio[3] = 0.33 * w;
    e->timings.ratio[4] = 0.36 * w;
    e->timings.ratio[5] = 0.38 * w;
    e->timings.ratio[6] = 0.0003;

    e->calculate_oscillators(e->timings);

    for (int x = 0; x < e->num_x; x++) {
        for (int y = 0; y < e->num_y; y++) {

            e->animation.dist = e->distance[x][y];
            e->animation.angle = e->polar_theta[x][y] + e->move.radial[1];
            e->animation.z = 5;
            e->animation.scale_x = 0.001;
            e->animation.scale_y = 0.1;
            e->animation.scale_z = 0.1;
            e->animation.offset_y = -10 * e->move.linear[0];
            e->animation.offset_x = 20;
            e->animation.offset_z = 10;
            e->animation.low_limit = 0;
            e->show1 = e->render_value(e->animation);

            e->pixel.red = 10 * e->show1;
            e->pixel.green = 0;
            e->pixel.blue = 0;

            e->pixel = e->rgb_sanity_check(e->pixel);

            e->setPixelColorInternal(x, y, e->pixel);
        }
    }
}
inline void Module_Experiment10(Context &ctx) {
    auto *e = ctx.mEngine;
    e->get_ready();

    e->timings.master_speed = 0.01;

    float w = 1;

    e->timings.ratio[0] = 0.01;
    e->timings.ratio[1] = 0.011;
    e->timings.ratio[2] = 0.013;
    e->timings.ratio[3] = 0.33 * w;
    e->timings.ratio[4] = 0.36 * w;
    e->timings.ratio[5] = 0.38 * w;
    e->timings.ratio[6] = 0.0003;

    e->timings.offset[0] = 0;
    e->timings.offset[1] = 100;
    e->timings.offset[2] = 200;
    e->timings.offset[3] = 300;
    e->timings.offset[4] = 400;
    e->timings.offset[5] = 500;
    e->timings.offset[6] = 600;

    e->calculate_oscillators(e->timings);

    for (int x = 0; x < e->num_x; x++) {
        for (int y = 0; y < e->num_y; y++) {

            float s = 0.4;
            float r = 1.5;

            e->animation.dist =
                3 + e->distance[x][y] +
                3 * fl::sinf(0.25 * e->distance[x][y] - e->move.radial[3]);
            e->animation.angle = e->polar_theta[x][y] + e->move.noise_angle[0] +
                                 e->move.noise_angle[6];
            e->animation.z = 5;
            e->animation.scale_x = 0.1 * s;
            e->animation.scale_y = 0.1 * s;
            e->animation.offset_z = 10 * e->move.linear[0];
            e->animation.offset_y = -5 * r * e->move.linear[0];
            e->animation.offset_x = 10;
            e->animation.low_limit = 0;
            e->show1 = e->render_value(e->animation);

            e->animation.dist =
                4 + e->distance[x][y] +
                4 * fl::sinf(0.24 * e->distance[x][y] - e->move.radial[4]);
            e->animation.angle = e->polar_theta[x][y] + e->move.noise_angle[1] +
                                 e->move.noise_angle[6];
            e->animation.z = 5;
            e->animation.scale_x = 0.1 * s;
            e->animation.scale_y = 0.1 * s;
            e->animation.offset_z = 0.1 * e->move.linear[1];
            e->animation.offset_y = -5 * r * e->move.linear[1];
            e->animation.offset_x = 100;
            e->animation.low_limit = 0;
            e->show2 = e->render_value(e->animation);

            e->animation.dist =
                5 + e->distance[x][y] +
                5 * fl::sinf(0.23 * e->distance[x][y] - e->move.radial[5]);
            e->animation.angle = e->polar_theta[x][y] + e->move.noise_angle[2] +
                                 e->move.noise_angle[6];
            e->animation.z = 5;
            e->animation.scale_x = 0.1 * s;
            e->animation.scale_y = 0.1 * s;
            e->animation.offset_z = 0.1 * e->move.linear[2];
            e->animation.offset_y = -5 * r * e->move.linear[2];
            e->animation.offset_x = 1000;
            e->animation.low_limit = 0;
            e->show3 = e->render_value(e->animation);

            e->show4 = e->colordodge(e->show1, e->show2);

            float rad = fl::sinf(PI / 2 +
                             e->distance[x][y] / 14);

            CHSV(rad * ((e->show1 + e->show2) + e->show3), 255, 255);

            e->pixel = e->rgb_sanity_check(e->pixel);

            fl::u8 a = e->getTime() / 100;
            CRGB p = CRGB(CHSV(((a + e->show1 + e->show2) + e->show3), 255, 255));
            animartrix_detail::rgb pixel;
            pixel.red = p.red;
            pixel.green = p.green;
            pixel.blue = p.blue;
            e->setPixelColorInternal(x, y, pixel);
        }
    }
}
inline void Fluffy_Blobs(Context &ctx) {
    auto *e = ctx.mEngine;

    e->timings.master_speed = 0.015;
    float size = 0.15;
    float radial_speed = 1;
    float linear_speed = 5;

    e->timings.ratio[0] = 0.025;
    e->timings.ratio[1] = 0.026;
    e->timings.ratio[2] = 0.027;
    e->timings.ratio[3] = 0.028;
    e->timings.ratio[4] = 0.029;
    e->timings.ratio[5] = 0.030;
    e->timings.ratio[6] = 0.031;
    e->timings.ratio[7] = 0.032;
    e->timings.ratio[8] = 0.033;

    e->calculate_oscillators(e->timings);

    for (int x = 0; x < e->num_x; x++) {
        for (int y = 0; y < e->num_y; y++) {

            e->animation.dist = e->distance[x][y];
            e->animation.angle = e->polar_theta[x][y] + (radial_speed * e->move.radial[0]);
            e->animation.z = 5;
            e->animation.scale_x = size;
            e->animation.scale_y = size;
            e->animation.offset_z = 0;
            e->animation.offset_x = 0;
            e->animation.offset_y = linear_speed * e->move.linear[0];
            e->animation.low_limit = 0;
            e->animation.high_limit = 1;
            e->show1 = e->render_value(e->animation);

            e->animation.angle = e->polar_theta[x][y] + (radial_speed * e->move.radial[1]);
            e->animation.offset_y = linear_speed * e->move.linear[1];
            e->animation.offset_z = 200;
            e->animation.scale_x = size * 1.1;
            e->animation.scale_y = size * 1.1;
            e->show2 = e->render_value(e->animation);

            e->animation.angle = e->polar_theta[x][y] + (radial_speed * e->move.radial[2]);
            e->animation.offset_y = linear_speed * e->move.linear[2];
            e->animation.offset_z = 400;
            e->animation.scale_x = size * 1.2;
            e->animation.scale_y = size * 1.2;
            e->show3 = e->render_value(e->animation);

            e->animation.angle = e->polar_theta[x][y] + (radial_speed * e->move.radial[3]);
            e->animation.offset_y = linear_speed * e->move.linear[3];
            e->animation.offset_z = 600;
            e->animation.scale_x = size;
            e->animation.scale_y = size;
            e->show4 = e->render_value(e->animation);

            e->animation.angle = e->polar_theta[x][y] + (radial_speed * e->move.radial[4]);
            e->animation.offset_y = linear_speed * e->move.linear[4];
            e->animation.offset_z = 800;
            e->animation.scale_x = size * 1.1;
            e->animation.scale_y = size * 1.1;
            e->show5 = e->render_value(e->animation);

            e->animation.angle = e->polar_theta[x][y] + (radial_speed * e->move.radial[5]);
            e->animation.offset_y = linear_speed * e->move.linear[5];
            e->animation.offset_z = 1800;
            e->animation.scale_x = size * 1.2;
            e->animation.scale_y = size * 1.2;
            e->show6 = e->render_value(e->animation);

            e->animation.angle = e->polar_theta[x][y] + (radial_speed * e->move.radial[6]);
            e->animation.offset_y = linear_speed * e->move.linear[6];
            e->animation.offset_z = 2800;
            e->animation.scale_x = size;
            e->animation.scale_y = size;
            e->show7 = e->render_value(e->animation);

            e->animation.angle = e->polar_theta[x][y] + (radial_speed * e->move.radial[7]);
            e->animation.offset_y = linear_speed * e->move.linear[7];
            e->animation.offset_z = 3800;
            e->animation.scale_x = size * 1.1;
            e->animation.scale_y = size * 1.1;
            e->show8 = e->render_value(e->animation);

            e->animation.angle = e->polar_theta[x][y] + (radial_speed * e->move.radial[8]);
            e->animation.offset_y = linear_speed * e->move.linear[8];
            e->animation.offset_z = 4800;
            e->animation.scale_x = size * 1.2;
            e->animation.scale_y = size * 1.2;
            e->show9 = e->render_value(e->animation);

            e->pixel.red = 0.8 * (e->show1 + e->show2 + e->show3) + (e->show4 + e->show5 + e->show6);
            e->pixel.green = 0.8 * (e->show4 + e->show5 + e->show6);
            e->pixel.blue = 0.3 * (e->show7 + e->show8 + e->show9);

            e->pixel = e->rgb_sanity_check(e->pixel);

            e->setPixelColorInternal(x, y, e->pixel);
        }
    }
}


} // namespace animartrix2_detail

// Q31 optimized Chasing_Spirals extracted to its own file.
// Included after main namespace so all types are defined.
#define ANIMARTRIX2_CHASING_SPIRALS_INTERNAL
#include "fl/fx/2d/animartrix2_detail/chasing_spirals.hpp" // allow-include-after-namespace

#if FL_ANIMARTRIX_USES_FAST_MATH
FL_OPTIMIZATION_LEVEL_O3_END
FL_FAST_MATH_END
#endif
