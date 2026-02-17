// Chasing_Spirals visualizer - all three variants (float, Q31, SIMD)
//
// This file consolidates all three implementations:
// 1. Chasing_Spirals: Original floating-point reference implementation
// 2. Chasing_Spirals_Q31: Fixed-point scalar (2.7x speedup over float)
// 3. Chasing_Spirals_Q31_SIMD: SIMD vectorized 4-wide (3.2x speedup, optimization target)

#include "fl/align.h"  // Required for FL_ALIGNAS before animartrix2_detail.hpp
#include "fl/fx/2d/animartrix2_detail.hpp"
#include "fl/fx/2d/animartrix2_detail/viz/chasing_spirals.h"
#include "fl/fx/2d/animartrix2_detail/chasing_spirals_common.hpp"

namespace fl {

// ============================================================================
// Variant 1: Chasing_Spirals (Float Reference Implementation)
// ============================================================================
// Original floating-point implementation from animartrix2.
// Baseline for performance comparisons.

void Chasing_Spirals(Context &ctx) {
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

// ============================================================================
// Variant 2: Chasing_Spirals_Q31 (Fixed-Point Scalar Implementation)
// ============================================================================
// Scalar fixed-point implementation using s16x16 Q31 math.
// Provides 2.7x speedup over float reference by avoiding FP operations.

namespace detail {
    using FP = fl::s16x16;
    using Perlin = fl::perlin_s16x16;
    using PixelLUT = fl::ChasingSpiralPixelLUT;
}  // namespace detail

void Chasing_Spirals_Q31(fl::Context &ctx) {
    // Common frame setup: timing, constants, LUTs
    auto setup = setupChasingSpiralFrame(ctx);
    const int total_pixels = setup.total_pixels;
    const detail::PixelLUT *lut = setup.lut;
    const i32 *fade_lut = setup.fade_lut;
    const u8 *perm = setup.perm;
    const i32 cx_raw = setup.cx_raw;
    const i32 cy_raw = setup.cy_raw;
    const i32 lin0_raw = setup.lin0_raw;
    const i32 lin1_raw = setup.lin1_raw;
    const i32 lin2_raw = setup.lin2_raw;
    const i32 rad0_raw = setup.rad0_raw;
    const i32 rad1_raw = setup.rad1_raw;
    const i32 rad2_raw = setup.rad2_raw;
    CRGB *leds = setup.leds;

    constexpr i32 FP_ONE = 1 << detail::FP::FRAC_BITS;

    // Computes one noise channel: sincos → cartesian → Perlin → clamp → scale.
    // Uses full sin32/cos32 precision (31-bit) for the coordinate computation
    // to reduce truncation error vs converting to s16x16 first.
    // Returns s16x16 raw value representing [0, 255].
    //
    // Precision analysis:
    //   sin32/cos32 output: 31-bit signed (range ±2^31, effectively ±1.0)
    //   dist_raw: s16x16 (16 frac bits, typically small values 0..~22)
    //   Product: (sin32_val * dist_raw) uses i64, shift by 31 → s16x16 format
    //   This preserves 15 more bits than the s16x16 sincos path.
    constexpr i32 RAD_TO_A24 = 2670177; // 256/(2*PI) in s16x16

    auto noise_channel = [&](i32 base_raw, i32 rad_raw,
                             i32 lin_raw, i32 dist_raw, int /*pixel_idx*/) -> i32 {
        // Convert angle (s16x16 radians) to sin32/cos32 input format
        u32 a24 = static_cast<u32>(
            (static_cast<i64>(base_raw + rad_raw) * RAD_TO_A24) >> detail::FP::FRAC_BITS);
        SinCos32 sc = sincos32(a24);
        // Coordinate computation with 31-bit trig precision
        i32 nx = lin_raw + cx_raw -
            static_cast<i32>((static_cast<i64>(sc.cos_val) * dist_raw) >> 31);
        i32 ny = cy_raw -
            static_cast<i32>((static_cast<i64>(sc.sin_val) * dist_raw) >> 31);

        i32 raw = detail::Perlin::pnoise2d_raw(nx, ny, fade_lut, perm);
        if (raw < 0) raw = 0;
        if (raw > FP_ONE) raw = FP_ONE;
        return raw * 255;
    };

    for (int i = 0; i < total_pixels; i++) {
        const detail::PixelLUT &px = lut[i];
        const i32 base_raw = px.base_angle.raw();
        const i32 dist_raw = px.dist_scaled.raw();

        // Three noise channels (explicitly unrolled)
        i32 s0 = noise_channel(base_raw, rad0_raw, lin0_raw, dist_raw, i);
        i32 s1 = noise_channel(base_raw, rad1_raw, lin1_raw, dist_raw, i);
        i32 s2 = noise_channel(base_raw, rad2_raw, lin2_raw, dist_raw, i);

        // Apply radial filter: (show * rf) >> 32 gives final u8 value.
        i32 r = static_cast<i32>((static_cast<i64>(s0) * px.rf3.raw()) >> (detail::FP::FRAC_BITS * 2));
        i32 g = static_cast<i32>((static_cast<i64>(s1) * px.rf_half.raw()) >> (detail::FP::FRAC_BITS * 2));
        i32 b = static_cast<i32>((static_cast<i64>(s2) * px.rf_quarter.raw()) >> (detail::FP::FRAC_BITS * 2));

        if (r < 0) r = 0; if (r > 255) r = 255;
        if (g < 0) g = 0; if (g > 255) g = 255;
        if (b < 0) b = 0; if (b > 255) b = 255;

        leds[px.pixel_idx] = CRGB(static_cast<u8>(r),
                                   static_cast<u8>(g),
                                   static_cast<u8>(b));
    }
}

// ============================================================================
// Variant 3: Chasing_Spirals_Q31_SIMD (SIMD Vectorized 4-Wide Implementation)
// ============================================================================
// SIMD-optimized version using sincos32_simd and pnoise2d_raw_simd4.
// Processes 4 pixels at once with batched trig (3 SIMD calls) and batched Perlin (3 SIMD calls).
// Provides 3.2x speedup over float reference, 1.3x speedup over Q31 scalar.
// PRIMARY OPTIMIZATION TARGET for chasing spirals.

void Chasing_Spirals_Q31_SIMD(fl::Context &ctx) {
    // ========== 1. Frame Setup ==========
    // Compute timing, constants, build PixelLUT, initialize fade LUT
    auto setup = setupChasingSpiralFrame(ctx);
    const int total_pixels = setup.total_pixels;
    const detail::PixelLUT *lut = setup.lut;
    const i32 *fade_lut = setup.fade_lut;
    const u8 *perm = setup.perm;
    const i32 cx_raw = setup.cx_raw;
    const i32 cy_raw = setup.cy_raw;
    const i32 lin0_raw = setup.lin0_raw;
    const i32 lin1_raw = setup.lin1_raw;
    const i32 lin2_raw = setup.lin2_raw;
    const i32 rad0_raw = setup.rad0_raw;
    const i32 rad1_raw = setup.rad1_raw;
    const i32 rad2_raw = setup.rad2_raw;
    CRGB *leds = setup.leds;

    // ========== 2. SIMD Pixel Pipeline (4-wide batches) ==========
    // Process 4 pixels at once: angle → sincos → Perlin → radial filter → output
    int i = 0;
    for (; i + 3 < total_pixels; i += 4) {
        // Load base angles and distances for 4 pixels into arrays
        i32 base_arr[4] = {
            lut[i+0].base_angle.raw(),
            lut[i+1].base_angle.raw(),
            lut[i+2].base_angle.raw(),
            lut[i+3].base_angle.raw()
        };
        i32 dist_arr[4] = {
            lut[i+0].dist_scaled.raw(),
            lut[i+1].dist_scaled.raw(),
            lut[i+2].dist_scaled.raw(),
            lut[i+3].dist_scaled.raw()
        };

        // Process RGB channels using unified SIMD pipeline
        i32 s_r[4], s_g[4], s_b[4];
        simd4_processChannel(base_arr, dist_arr, rad0_raw, lin0_raw, fade_lut, perm, cx_raw, cy_raw, s_r);
        simd4_processChannel(base_arr, dist_arr, rad1_raw, lin1_raw, fade_lut, perm, cx_raw, cy_raw, s_g);
        simd4_processChannel(base_arr, dist_arr, rad2_raw, lin2_raw, fade_lut, perm, cx_raw, cy_raw, s_b);

        // Extract individual pixel values for radial filter application
        i32 s0_r = s_r[0], s1_r = s_r[1], s2_r = s_r[2], s3_r = s_r[3];
        i32 s0_g = s_g[0], s1_g = s_g[1], s2_g = s_g[2], s3_g = s_g[3];
        i32 s0_b = s_b[0], s1_b = s_b[1], s2_b = s_b[2], s3_b = s_b[3];

        // Apply radial filter and clamp for all 4 pixels
        i32 r0 = applyRadialFilter(s0_r, lut[i+0].rf3.raw());
        i32 g0 = applyRadialFilter(s0_g, lut[i+0].rf_half.raw());
        i32 b0 = applyRadialFilter(s0_b, lut[i+0].rf_quarter.raw());

        i32 r1 = applyRadialFilter(s1_r, lut[i+1].rf3.raw());
        i32 g1 = applyRadialFilter(s1_g, lut[i+1].rf_half.raw());
        i32 b1 = applyRadialFilter(s1_b, lut[i+1].rf_quarter.raw());

        i32 r2 = applyRadialFilter(s2_r, lut[i+2].rf3.raw());
        i32 g2 = applyRadialFilter(s2_g, lut[i+2].rf_half.raw());
        i32 b2 = applyRadialFilter(s2_b, lut[i+2].rf_quarter.raw());

        i32 r3 = applyRadialFilter(s3_r, lut[i+3].rf3.raw());
        i32 g3 = applyRadialFilter(s3_g, lut[i+3].rf_half.raw());
        i32 b3 = applyRadialFilter(s3_b, lut[i+3].rf_quarter.raw());

        // Write outputs
        leds[lut[i+0].pixel_idx] = CRGB(static_cast<u8>(r0), static_cast<u8>(g0), static_cast<u8>(b0));
        leds[lut[i+1].pixel_idx] = CRGB(static_cast<u8>(r1), static_cast<u8>(g1), static_cast<u8>(b1));
        leds[lut[i+2].pixel_idx] = CRGB(static_cast<u8>(r2), static_cast<u8>(g2), static_cast<u8>(b2));
        leds[lut[i+3].pixel_idx] = CRGB(static_cast<u8>(r3), static_cast<u8>(g3), static_cast<u8>(b3));
    }

    // Scalar fallback for remaining pixels (when total_pixels % 4 != 0)
    auto noise_channel = [&](i32 base_raw, i32 rad_raw,
                             i32 lin_raw, i32 dist_raw) -> i32 {
        u32 a24 = radiansToA24(base_raw, rad_raw);
        SinCos32 sc = sincos32(a24);
        i32 nx = lin_raw + cx_raw -
            static_cast<i32>((static_cast<i64>(sc.cos_val) * dist_raw) >> 31);
        i32 ny = cy_raw -
            static_cast<i32>((static_cast<i64>(sc.sin_val) * dist_raw) >> 31);
        i32 raw = detail::Perlin::pnoise2d_raw(nx, ny, fade_lut, perm);
        return clampAndScale255(raw);
    };

    for (; i < total_pixels; i++) {
        const detail::PixelLUT &px = lut[i];
        const i32 base_raw = px.base_angle.raw();
        const i32 dist_raw = px.dist_scaled.raw();

        i32 s0 = noise_channel(base_raw, rad0_raw, lin0_raw, dist_raw);
        i32 s1 = noise_channel(base_raw, rad1_raw, lin1_raw, dist_raw);
        i32 s2 = noise_channel(base_raw, rad2_raw, lin2_raw, dist_raw);

        i32 r = applyRadialFilter(s0, px.rf3.raw());
        i32 g = applyRadialFilter(s1, px.rf_half.raw());
        i32 b = applyRadialFilter(s2, px.rf_quarter.raw());

        leds[px.pixel_idx] = CRGB(static_cast<u8>(r), static_cast<u8>(g), static_cast<u8>(b));
    }
}

} // namespace fl
