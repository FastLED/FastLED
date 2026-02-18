// Chasing_Spirals implementations (float, Q31 scalar, Q31 SIMD)

#include "fl/align.h"
#include "fl/fx/2d/animartrix2_detail/engine.h"
#include "fl/fx/2d/animartrix2_detail/perlin_s16x16.h"
#include "fl/fx/2d/animartrix2_detail/perlin_s16x16_simd.h"
#include "fl/fx/2d/animartrix2_detail/perlin_float.h"
#include "fl/fx/2d/animartrix2_detail/chasing_spiral_pixel_lut.h"
#include "fl/simd.h"
#include "fl/sin32.h"
#include "fl/fx/2d/animartrix2_detail/chasing_spirals.h"

namespace fl {

namespace {

using FP = fl::s16x16;
using Perlin = perlin_s16x16;
using PixelLUT = ChasingSpiralPixelLUT;

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
u32 radiansToA24(i32 base_s16x16, i32 offset_s16x16) {
    constexpr i32 RAD_TO_A24 = 2670177;
    return static_cast<u32>((static_cast<i64>(base_s16x16 + offset_s16x16) * RAD_TO_A24) >> FP::FRAC_BITS);
}

// Compute Perlin coordinates from SIMD sincos results and distances (4 pixels)
void simd4_computePerlinCoords(
    const i32 cos_arr[4], const i32 sin_arr[4],
    const i32 dist_arr[4], i32 lin_raw, i32 cx_raw, i32 cy_raw,
    i32 nx_out[4], i32 ny_out[4]) {
    for (int i = 0; i < 4; i++) {
        nx_out[i] = lin_raw + cx_raw - static_cast<i32>((static_cast<i64>(cos_arr[i]) * dist_arr[i]) >> 31);
        ny_out[i] = cy_raw - static_cast<i32>((static_cast<i64>(sin_arr[i]) * dist_arr[i]) >> 31);
    }
}

// Clamp s16x16 value to [0, 1] and scale to [0, 255]
i32 clampAndScale255(i32 raw_s16x16) {
    constexpr i32 FP_ONE = 1 << FP::FRAC_BITS;
    if (raw_s16x16 < 0) raw_s16x16 = 0;
    if (raw_s16x16 > FP_ONE) raw_s16x16 = FP_ONE;
    return raw_s16x16 * 255;
}

// Apply radial filter to noise value and clamp to [0, 255]
i32 applyRadialFilter(i32 noise_255, i32 rf_raw) {
    i32 result = static_cast<i32>((static_cast<i64>(noise_255) * rf_raw) >> (FP::FRAC_BITS * 2));
    if (result < 0) result = 0;
    if (result > 255) result = 255;
    return result;
}

// Process one color channel for 4 pixels using SIMD (angle -> sincos -> Perlin -> clamp)
void simd4_processChannel(
    const i32 base_arr[4], const i32 dist_arr[4],
    i32 radial_offset, i32 linear_offset,
    const i32 *fade_lut, const u8 *perm, i32 cx_raw, i32 cy_raw,
    i32 noise_out[4]) {

    constexpr i32 RAD_TO_A24 = 2670177;

    // Load base angles and add radial offset (static_cast i32->u32 for SIMD)
    u32 base_u32[4] = {
        static_cast<u32>(base_arr[0]), static_cast<u32>(base_arr[1]),
        static_cast<u32>(base_arr[2]), static_cast<u32>(base_arr[3])
    };
    simd::simd_u32x4 base_vec = simd::load_u32_4(base_u32);
    simd::simd_u32x4 offset_vec = simd::set1_u32_4(static_cast<u32>(radial_offset));
    simd::simd_u32x4 sum_vec = simd::add_i32_4(base_vec, offset_vec);

    // Multiply by constant and shift right by 16 (Q16.16 -> A24)
    simd::simd_u32x4 rad_const_vec = simd::set1_u32_4(static_cast<u32>(RAD_TO_A24));
    simd::simd_u32x4 angles_vec = simd::mulhi_su32_4(sum_vec, rad_const_vec);
    SinCos32_simd sc = sincos32_simd(angles_vec);

    // Extract sin/cos results to arrays (store as u32, static_cast to i32)
    u32 cos_u32[4], sin_u32[4];
    simd::store_u32_4(cos_u32, sc.cos_vals);
    simd::store_u32_4(sin_u32, sc.sin_vals);
    i32 cos_arr_local[4] = {
        static_cast<i32>(cos_u32[0]), static_cast<i32>(cos_u32[1]),
        static_cast<i32>(cos_u32[2]), static_cast<i32>(cos_u32[3])
    };
    i32 sin_arr_local[4] = {
        static_cast<i32>(sin_u32[0]), static_cast<i32>(sin_u32[1]),
        static_cast<i32>(sin_u32[2]), static_cast<i32>(sin_u32[3])
    };

    // Compute Perlin coordinates from sincos and distances
    i32 nx[4], ny[4];
    simd4_computePerlinCoords(cos_arr_local, sin_arr_local, dist_arr, linear_offset, cx_raw, cy_raw, nx, ny);

    // SIMD Perlin noise (4 evaluations in parallel)
    i32 raw_noise[4];
    perlin_s16x16_simd::pnoise2d_raw_simd4(nx, ny, fade_lut, perm, raw_noise);

    // Clamp and scale results to [0, 255]
    noise_out[0] = clampAndScale255(raw_noise[0]);
    noise_out[1] = clampAndScale255(raw_noise[1]);
    noise_out[2] = clampAndScale255(raw_noise[2]);
    noise_out[3] = clampAndScale255(raw_noise[3]);
}

// Extract common frame setup logic shared by both variants.
// Computes timing, scaled constants, builds PixelLUT, initializes fade LUT.
FrameSetup setupChasingSpiralFrame(Context &ctx) {
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

    // Per-frame constants (float->FP boundary conversions)
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
    const FP linear0_scaled = FP(fmodf(e->move.linear[0], perlin_period) * scale_f);
    const FP linear1_scaled = FP(fmodf(e->move.linear[1], perlin_period) * scale_f);
    const FP linear2_scaled = FP(fmodf(e->move.linear[2], perlin_period) * scale_f);

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
    const i32 *fade_lut = e->mFadeLUT;

    // Permutation table for Perlin noise
    const u8 *perm = PERLIN_NOISE;

    // Precompute raw i32 values for per-frame constants to avoid
    // repeated s16x16 construction overhead in the inner loop.
    const i32 cx_raw = center_x_scaled.raw();
    const i32 cy_raw = center_y_scaled.raw();
    const i32 lin0_raw = linear0_scaled.raw();
    const i32 lin1_raw = linear1_scaled.raw();
    const i32 lin2_raw = linear2_scaled.raw();
    const i32 rad0_raw = radial0.raw();
    const i32 rad1_raw = radial1.raw();
    const i32 rad2_raw = radial2.raw();

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

} // anonymous namespace

// ============================================================================
// Float Implementation (original algorithm, uses v2 Engine)
// ============================================================================

void Chasing_Spirals_Float(Context &ctx) {
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
                3 * e->polar_theta[x][y] + e->move.radial[0] -
                e->distance[x][y] / 3;
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
                3 * e->polar_theta[x][y] + e->move.radial[1] -
                e->distance[x][y] / 3;
            e->animation.dist = e->distance[x][y];
            e->animation.offset_x = e->move.linear[1];
            float show2 = e->render_value(e->animation);

            e->animation.angle =
                3 * e->polar_theta[x][y] + e->move.radial[2] -
                e->distance[x][y] / 3;
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
// Q31 Scalar Implementation (fixed-point, non-vectorized)
// ============================================================================

void Chasing_Spirals_Q31(Context &ctx) {
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

    constexpr i32 FP_ONE = 1 << FP::FRAC_BITS;
    constexpr i32 RAD_TO_A24 = 2670177; // 256/(2*PI) in s16x16

    auto noise_channel = [&](i32 base_raw, i32 rad_raw,
                             i32 lin_raw, i32 dist_raw) -> i32 {
        u32 a24 = static_cast<u32>(
            (static_cast<i64>(base_raw + rad_raw) * RAD_TO_A24) >> FP::FRAC_BITS);
        SinCos32 sc = sincos32(a24);
        i32 nx = lin_raw + cx_raw -
            static_cast<i32>((static_cast<i64>(sc.cos_val) * dist_raw) >> 31);
        i32 ny = cy_raw -
            static_cast<i32>((static_cast<i64>(sc.sin_val) * dist_raw) >> 31);

        i32 raw = Perlin::pnoise2d_raw(nx, ny, fade_lut, perm);
        if (raw < 0) raw = 0;
        if (raw > FP_ONE) raw = FP_ONE;
        return raw * 255;
    };

    for (int i = 0; i < total_pixels; i++) {
        const PixelLUT &px = lut[i];
        const i32 base_raw = px.base_angle.raw();
        const i32 dist_raw = px.dist_scaled.raw();

        i32 s0 = noise_channel(base_raw, rad0_raw, lin0_raw, dist_raw);
        i32 s1 = noise_channel(base_raw, rad1_raw, lin1_raw, dist_raw);
        i32 s2 = noise_channel(base_raw, rad2_raw, lin2_raw, dist_raw);

        i32 r = static_cast<i32>((static_cast<i64>(s0) * px.rf3.raw()) >> (FP::FRAC_BITS * 2));
        i32 g = static_cast<i32>((static_cast<i64>(s1) * px.rf_half.raw()) >> (FP::FRAC_BITS * 2));
        i32 b = static_cast<i32>((static_cast<i64>(s2) * px.rf_quarter.raw()) >> (FP::FRAC_BITS * 2));

        if (r < 0) r = 0; if (r > 255) r = 255;
        if (g < 0) g = 0; if (g > 255) g = 255;
        if (b < 0) b = 0; if (b > 255) b = 255;

        leds[px.pixel_idx] = CRGB(static_cast<u8>(r),
                                   static_cast<u8>(g),
                                   static_cast<u8>(b));
    }
}

// ============================================================================
// SIMD Implementation (vectorized 4-wide processing)
// ============================================================================

void Chasing_Spirals_Q31_SIMD(Context &ctx) {
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

    // SIMD Pixel Pipeline (4-wide batches)
    int i = 0;
    for (; i + 3 < total_pixels; i += 4) {
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

        i32 s_r[4], s_g[4], s_b[4];
        simd4_processChannel(base_arr, dist_arr, rad0_raw, lin0_raw, fade_lut, perm, cx_raw, cy_raw, s_r);
        simd4_processChannel(base_arr, dist_arr, rad1_raw, lin1_raw, fade_lut, perm, cx_raw, cy_raw, s_g);
        simd4_processChannel(base_arr, dist_arr, rad2_raw, lin2_raw, fade_lut, perm, cx_raw, cy_raw, s_b);

        i32 s0_r = s_r[0], s1_r = s_r[1], s2_r = s_r[2], s3_r = s_r[3];
        i32 s0_g = s_g[0], s1_g = s_g[1], s2_g = s_g[2], s3_g = s_g[3];
        i32 s0_b = s_b[0], s1_b = s_b[1], s2_b = s_b[2], s3_b = s_b[3];

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

        leds[lut[i+0].pixel_idx] = CRGB(static_cast<u8>(r0), static_cast<u8>(g0), static_cast<u8>(b0));
        leds[lut[i+1].pixel_idx] = CRGB(static_cast<u8>(r1), static_cast<u8>(g1), static_cast<u8>(b1));
        leds[lut[i+2].pixel_idx] = CRGB(static_cast<u8>(r2), static_cast<u8>(g2), static_cast<u8>(b2));
        leds[lut[i+3].pixel_idx] = CRGB(static_cast<u8>(r3), static_cast<u8>(g3), static_cast<u8>(b3));
    }

    // Scalar fallback for remaining pixels (when total_pixels % 4 != 0)
    for (; i < total_pixels; i++) {
        const PixelLUT &px = lut[i];
        const i32 base_raw = px.base_angle.raw();
        const i32 dist_raw = px.dist_scaled.raw();

        auto noise_ch = [&](i32 rad_raw, i32 lin_raw) -> i32 {
            u32 a24 = radiansToA24(base_raw, rad_raw);
            SinCos32 sc = sincos32(a24);
            i32 nx = lin_raw + cx_raw -
                static_cast<i32>((static_cast<i64>(sc.cos_val) * dist_raw) >> 31);
            i32 ny = cy_raw -
                static_cast<i32>((static_cast<i64>(sc.sin_val) * dist_raw) >> 31);
            i32 raw = Perlin::pnoise2d_raw(nx, ny, fade_lut, perm);
            return clampAndScale255(raw);
        };

        i32 s0 = noise_ch(rad0_raw, lin0_raw);
        i32 s1 = noise_ch(rad1_raw, lin1_raw);
        i32 s2 = noise_ch(rad2_raw, lin2_raw);

        i32 r = applyRadialFilter(s0, px.rf3.raw());
        i32 g = applyRadialFilter(s1, px.rf_half.raw());
        i32 b = applyRadialFilter(s2, px.rf_quarter.raw());

        leds[px.pixel_idx] = CRGB(static_cast<u8>(r), static_cast<u8>(g), static_cast<u8>(b));
    }
}

} // namespace fl
