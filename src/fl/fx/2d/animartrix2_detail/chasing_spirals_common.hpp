#pragma once

// Chasing_Spirals shared code - common helpers and structures
//
// Shared by all Chasing_Spirals implementations (scalar, SIMD, etc.)
// DO NOT include directly - included by implementation files

#ifndef ANIMARTRIX2_CHASING_SPIRALS_INTERNAL
#error "Do not include chasing_spirals_common.hpp directly. Include animartrix2.hpp instead."
#endif

namespace animartrix2_detail {

// Q31 fixed-point helper types (avoid polluting global namespace)
namespace {  // Anonymous namespace for file-local types
    using FP = fl::s16x16;
    using Perlin = perlin_s16x16;
    using PixelLUT = ChasingSpiralPixelLUT;
}

// Common setup values returned by setupChasingSpiralFrame
struct FrameSetup {
    int total_pixels;
    const PixelLUT *lut;
    const fl::i32 *fade_lut;
    const fl::u8 *perm;
    fl::i32 cx_raw;
    fl::i32 cy_raw;
    fl::i32 lin0_raw;
    fl::i32 lin1_raw;
    fl::i32 lin2_raw;
    fl::i32 rad0_raw;
    fl::i32 rad1_raw;
    fl::i32 rad2_raw;
    CRGB *leds;
};

// Convert s16x16 angle (radians) to A24 format for sincos32
inline fl::u32 radiansToA24(fl::i32 base_s16x16, fl::i32 offset_s16x16) {
    constexpr fl::i32 RAD_TO_A24 = 2670177;  // Conversion constant for radian to A24 format
    return static_cast<fl::u32>((static_cast<fl::i64>(base_s16x16 + offset_s16x16) * RAD_TO_A24) >> FP::FRAC_BITS);
}

// Compute Perlin coordinates from SIMD sincos results and distances (4 pixels)
inline void simd4_computePerlinCoords(
    const fl::i32 cos_arr[4], const fl::i32 sin_arr[4],
    const fl::i32 dist_arr[4], fl::i32 lin_raw, fl::i32 cx_raw, fl::i32 cy_raw,
    fl::i32 nx_out[4], fl::i32 ny_out[4]) {

    for (int i = 0; i < 4; i++) {
        nx_out[i] = lin_raw + cx_raw - static_cast<fl::i32>((static_cast<fl::i64>(cos_arr[i]) * dist_arr[i]) >> 31);
        ny_out[i] = cy_raw - static_cast<fl::i32>((static_cast<fl::i64>(sin_arr[i]) * dist_arr[i]) >> 31);
    }
}

// Clamp s16x16 value to [0, 1] and scale to [0, 255]
inline fl::i32 clampAndScale255(fl::i32 raw_s16x16) {
    constexpr fl::i32 FP_ONE = 1 << FP::FRAC_BITS;
    if (raw_s16x16 < 0) raw_s16x16 = 0;
    if (raw_s16x16 > FP_ONE) raw_s16x16 = FP_ONE;
    return raw_s16x16 * 255;
}

// Apply radial filter to noise value and clamp to [0, 255]
inline fl::i32 applyRadialFilter(fl::i32 noise_255, fl::i32 rf_raw) {
    fl::i32 result = static_cast<fl::i32>((static_cast<fl::i64>(noise_255) * rf_raw) >> (FP::FRAC_BITS * 2));
    if (result < 0) result = 0;
    if (result > 255) result = 255;
    return result;
}

// Process one color channel for 4 pixels using SIMD (angle → sincos → Perlin → clamp)
inline void simd4_processChannel(
    const fl::i32 base_arr[4], const fl::i32 dist_arr[4],
    fl::i32 radial_offset, fl::i32 linear_offset,
    const fl::i32 *fade_lut, const fl::u8 *perm, fl::i32 cx_raw, fl::i32 cy_raw,
    fl::i32 noise_out[4]) {

    // Compute angles for 4 pixels using SIMD
    // radiansToA24: (base + offset) * RAD_TO_A24 >> 16
    constexpr fl::i32 RAD_TO_A24 = 2670177;

    // Load base angles and add radial offset
    fl::simd::simd_u32x4 base_vec = fl::simd::load_u32_4(reinterpret_cast<const fl::u32*>(base_arr)); // ok reinterpret cast
    fl::simd::simd_u32x4 offset_vec = fl::simd::set1_u32_4(static_cast<fl::u32>(radial_offset));
    fl::simd::simd_u32x4 sum_vec = fl::simd::add_i32_4(base_vec, offset_vec);

    // Multiply by constant and shift right by 16 (Q16.16 → A24)
    fl::simd::simd_u32x4 rad_const_vec = fl::simd::set1_u32_4(static_cast<fl::u32>(RAD_TO_A24));
    fl::simd::simd_u32x4 angles_vec = fl::simd::mulhi_su32_4(sum_vec, rad_const_vec);
    fl::SinCos32_simd sc = fl::sincos32_simd(angles_vec);

    // Extract sin/cos results to arrays
    fl::i32 cos_arr[4], sin_arr[4];
    fl::simd::store_u32_4(reinterpret_cast<fl::u32*>(cos_arr), sc.cos_vals); // ok reinterpret cast
    fl::simd::store_u32_4(reinterpret_cast<fl::u32*>(sin_arr), sc.sin_vals); // ok reinterpret cast

    // Compute Perlin coordinates from sincos and distances
    fl::i32 nx[4], ny[4];
    simd4_computePerlinCoords(cos_arr, sin_arr, dist_arr, linear_offset, cx_raw, cy_raw, nx, ny);

    // SIMD Perlin noise (4 evaluations in parallel)
    fl::i32 raw_noise[4];
    Perlin::pnoise2d_raw_simd4(nx, ny, fade_lut, perm, raw_noise);

    // Clamp and scale results to [0, 255]
    noise_out[0] = clampAndScale255(raw_noise[0]);
    noise_out[1] = clampAndScale255(raw_noise[1]);
    noise_out[2] = clampAndScale255(raw_noise[2]);
    noise_out[3] = clampAndScale255(raw_noise[3]);
}

// Extract common frame setup logic shared by all Chasing_Spirals variants.
// Computes timing, scaled constants, builds PixelLUT, initializes fade LUT.
inline FrameSetup setupChasingSpiralFrame(Context &ctx) {
    auto *e = ctx.mEngine;
    e->get_ready();

    // Timing (once per frame, float is fine here)
    e->timings.master_speed = 0.01;
    e->timings.ratio[0] = 0.1;
    e->timings.ratio[1] = 0.13;
    e->timings.ratio[2] = 0.16;
    e->timings.offset[1] = 10;
    e->timings.offset[2] = 20;
    e->timings.offset[3] = 30;
    e->calculate_oscillators(e->timings);

    const int num_x = e->num_x;
    const int num_y = e->num_y;
    const int total_pixels = num_x * num_y;

    // Per-frame constants (float→FP boundary conversions)
    constexpr FP scale(0.1f);
    const FP radius_fp(e->radial_filter_radius);
    const FP center_x_scaled = FP(e->animation.center_x * 0.1f);
    const FP center_y_scaled = FP(e->animation.center_y * 0.1f);

    const FP radial0(e->move.radial[0]);
    const FP radial1(e->move.radial[1]);
    const FP radial2(e->move.radial[2]);

    // Reduce linear offsets mod Perlin period to prevent s16x16 overflow,
    // then pre-multiply by scale (0.1) in float before single FP conversion.
    constexpr float perlin_period = 2560.0f; // 256.0f / 0.1f
    constexpr float scale_f = 0.1f;
    const FP linear0_scaled = FP(fl::fmodf(e->move.linear[0], perlin_period) * scale_f);
    const FP linear1_scaled = FP(fl::fmodf(e->move.linear[1], perlin_period) * scale_f);
    const FP linear2_scaled = FP(fl::fmodf(e->move.linear[2], perlin_period) * scale_f);

    constexpr FP three_fp(3.0f);
    constexpr FP one(1.0f);

    // Build per-pixel geometry LUT (once, persists across frames)
    if (e->mChasingSpiralLUT.size() != static_cast<size_t>(total_pixels)) {
        e->mChasingSpiralLUT.resize(total_pixels);
        const FP inv_radius = one / radius_fp;
        const FP one_third = one / three_fp;
        PixelLUT *lut = e->mChasingSpiralLUT.data();
        int idx = 0;
        for (int x = 0; x < num_x; x++) {
            for (int y = 0; y < num_y; y++) {
                const FP theta(e->polar_theta[x][y]);
                const FP dist(e->distance[x][y]);
                const FP rf = (radius_fp - dist) * inv_radius;
                lut[idx].base_angle = three_fp * theta - dist * one_third;
                lut[idx].dist_scaled = dist * scale;
                lut[idx].rf3 = three_fp * rf;
                lut[idx].rf_half = rf >> 1;
                lut[idx].rf_quarter = rf >> 2;
                lut[idx].pixel_idx = e->mCtx->xyMapFn(x, y, e->mCtx->xyMapUserData);
                idx++;
            }
        }
    }
    const PixelLUT *lut = e->mChasingSpiralLUT.data();

    // Build fade LUT (once per Engine lifetime)
    if (!e->mFadeLUTInitialized) {
        Perlin::init_fade_lut(e->mFadeLUT);
        e->mFadeLUTInitialized = true;
    }
    const fl::i32 *fade_lut = e->mFadeLUT;

    // Permutation table for Perlin noise
    const fl::u8 *perm = animartrix2_detail::PERLIN_NOISE;

    // Precompute raw i32 values for per-frame constants to avoid
    // repeated s16x16 construction overhead in the inner loop.
    const fl::i32 cx_raw = center_x_scaled.raw();
    const fl::i32 cy_raw = center_y_scaled.raw();
    const fl::i32 lin0_raw = linear0_scaled.raw();
    const fl::i32 lin1_raw = linear1_scaled.raw();
    const fl::i32 lin2_raw = linear2_scaled.raw();
    const fl::i32 rad0_raw = radial0.raw();
    const fl::i32 rad1_raw = radial1.raw();
    const fl::i32 rad2_raw = radial2.raw();

    CRGB *leds = e->mCtx->leds;

    return FrameSetup{
        total_pixels,
        lut,
        fade_lut,
        perm,
        cx_raw,
        cy_raw,
        lin0_raw,
        lin1_raw,
        lin2_raw,
        rad0_raw,
        rad1_raw,
        rad2_raw,
        leds
    };
}

} // namespace animartrix2_detail
