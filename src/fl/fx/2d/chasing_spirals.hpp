#pragma once

// Chasing_Spirals s16x16 fixed-point implementation.
// Replaces all inner-loop floating-point with integer math.
//
// Included from animartrix2_detail.hpp — do NOT include directly.

#ifndef ANIMARTRIX2_CHASING_SPIRALS_INTERNAL
#error "Do not include chasing_spirals.hpp directly. Include animartrix2.hpp instead."
#endif

// fl/fixed_point/s16x16.h is included by animartrix2_detail.hpp before
// the namespace block, so fl::s16x16 is available at global scope.

namespace animartrix2_detail {

// ---- Float reference (disabled, kept for comparison) -----------------------
#if 0
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
#endif // float Chasing_Spirals reference

// ============================================================
// s16x16 fixed-point Chasing_Spirals
// ============================================================

namespace q31 {

using FP = fl::s16x16;
using Perlin = animartrix2_detail::perlin_s16x16;
using PixelLUT = ChasingSpiralPixelLUT;

inline void Chasing_Spirals_Q31(Context &ctx) {
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

    // Per-frame constants
    const FP scale(0.1f);
    const FP radius_fp(e->radial_filter_radius);
    const FP center_x_scaled = FP(e->animation.center_x) * scale;
    const FP center_y_scaled = FP(e->animation.center_y) * scale;

    const FP radial0(e->move.radial[0]);
    const FP radial1(e->move.radial[1]);
    const FP radial2(e->move.radial[2]);

    // Reduce linear offsets mod Perlin period to prevent overflow.
    const float perlin_period = 256.0f / 0.1f;
    const FP linear0_scaled = FP(fl::fmodf(e->move.linear[0], perlin_period)) * scale;
    const FP linear1_scaled = FP(fl::fmodf(e->move.linear[1], perlin_period)) * scale;
    const FP linear2_scaled = FP(fl::fmodf(e->move.linear[2], perlin_period)) * scale;

    const FP three_fp(3.0f);
    const FP one(1.0f);
    const FP zero;

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
    const int32_t *fade_lut = e->mFadeLUT;

    // Permutation table for Perlin noise
    const uint8_t *perm = animartrix_detail::PERLIN_NOISE;

    // One noise layer: polar->cartesian, 2D Perlin, clamp to [0, 255].
    auto render_layer = [&](FP angle, FP dist_scaled,
                            FP offset_x_scaled) -> FP {
        FP cos_a, sin_a;
        FP::sincos(angle, sin_a, cos_a);
        FP newx = offset_x_scaled + center_x_scaled - cos_a * dist_scaled;
        FP newy = center_y_scaled - sin_a * dist_scaled;
        FP raw = Perlin::pnoise2d(newx, newy, fade_lut, perm);
        if (raw < zero) raw = zero;
        if (raw > one) raw = one;
        return raw * 255;
    };

    CRGB *leds = e->mCtx->leds;

    for (int i = 0; i < total_pixels; i++) {
        const PixelLUT &px = lut[i];

        FP show1 = render_layer(px.base_angle + radial0, px.dist_scaled, linear0_scaled);
        FP show2 = render_layer(px.base_angle + radial1, px.dist_scaled, linear1_scaled);
        FP show3 = render_layer(px.base_angle + radial2, px.dist_scaled, linear2_scaled);

        int32_t r = (show1 * px.rf3).to_int();
        int32_t g = (show2 * px.rf_half).to_int();
        int32_t b = (show3 * px.rf_quarter).to_int();

        if (r < 0) r = 0; if (r > 255) r = 255;
        if (g < 0) g = 0; if (g > 255) g = 255;
        if (b < 0) b = 0; if (b > 255) b = 255;

        leds[px.pixel_idx] = CRGB(static_cast<uint8_t>(r),
                                   static_cast<uint8_t>(g),
                                   static_cast<uint8_t>(b));
    }
}

} // namespace q31

} // namespace animartrix2_detail
