// Chasing_Spirals Q31 scalar implementation (fixed-point, non-vectorized)

#include "fl/align.h"  // Required for FL_ALIGNAS
#include "fl/fx/2d/animartrix2_detail/engine.h"
#include "fl/sin32.h"
#include "chasing_spirals_simd.h"

namespace fl {

namespace {
    using FP = fl::s16x16;
    using Perlin = perlin_s16x16;
    using PixelLUT = ChasingSpiralPixelLUT;
}

// SIMD-optimized version: Uses sincos32_simd and pnoise2d_raw_simd4 for vectorized processing.
// Processes 4 pixels at once with batched trig (3 SIMD calls) and batched Perlin (3 SIMD calls).
// Expected speedup: 15-20% over Batch4 by reducing sincos calls from 12 to 3 per batch.
void Chasing_Spirals_Q31_SIMD(Context &ctx) {
    // ========== 1. Frame Setup ==========
    // Compute timing, constants, build PixelLUT, initialize fade LUT
    auto setup = setupChasingSpiralFrame(ctx);
    const int total_pixels = setup.total_pixels;
    const PixelLUT *lut = setup.lut;
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
        i32 raw = Perlin::pnoise2d_raw(nx, ny, fade_lut, perm);
        return clampAndScale255(raw);
    };

    for (; i < total_pixels; i++) {
        const PixelLUT &px = lut[i];
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
