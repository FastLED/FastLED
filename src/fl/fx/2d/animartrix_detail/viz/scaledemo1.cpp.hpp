#include "fl/stl/compiler_control.h"
#include "fl/fx/2d/animartrix_detail/viz/scaledemo1.h"
#include "fl/fx/2d/animartrix_detail/render_value_fp.h"
#include "fl/fx/2d/animartrix_detail/perlin_float.h"

FL_FAST_MATH_BEGIN
FL_OPTIMIZATION_LEVEL_O3_BEGIN

namespace fl {

void Scaledemo1::draw(Context &ctx) {
    auto *e = ctx.mEngine.get();
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


// ============================================================================
// Fixed-Point Implementation of Scaledemo1
// ============================================================================

void Scaledemo1_FP::draw(Context &ctx) {
    using FP = fl::s16x16;

    auto *e = ctx.mEngine.get();
    e->get_ready();
    mState.ensureCache(e);
    const fl::i32 *fade_lut = fl::assume_aligned<16>(mState.fade_lut);
    const fl::u8 *perm = PERLIN_NOISE;

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

    const int total_pixels = mState.count;

    constexpr fl::i32 FP_ONE = static_cast<fl::i32>(1) << FP::FRAC_BITS;
    const fl::i32 cx_raw = FP(e->animation.center_x).raw();
    const fl::i32 cy_raw = FP(e->animation.center_y).raw();

    // dist = 0.3 * distance * 0.8 = 0.24 * distance
    constexpr fl::i32 dist_scale_raw = FP(0.24f).raw();

    // angle = 3 * polar_theta + radial[2]  (per-pixel: 3*theta is varying)
    const fl::i32 three_raw = FP(3.0f).raw();
    const fl::i32 radial2_raw = FP(e->move.radial[2]).raw();

    // scale_x and scale_y are per-frame (depend on noise_angle)
    const fl::i32 sx_raw = FP(0.1f + e->move.noise_angle[0] / 10.0f).raw();
    const fl::i32 sy_raw = FP(0.1f + e->move.noise_angle[1] / 10.0f).raw();

    // angle2 for second render_value call is constant = 3
    const fl::i32 angle2_raw = FP(3.0f).raw();

    // Distance cutoff: distance > 16 → black
    const fl::i32 dist_cutoff_raw = FP(16.0f).raw();

    render_parameters_fp p = {};
    p.scale_x_raw = sx_raw;
    p.scale_y_raw = sy_raw;
    p.scale_z_raw = FP(0.01f).raw();
    p.offset_x_raw = 0;
    p.offset_y_raw = 0;
    p.offset_z_raw = FP(100.0f * e->move.linear[0]).raw();
    p.z_raw = FP(30.0f).raw();
    p.center_x_raw = cx_raw;
    p.center_y_raw = cy_raw;
    p.low_limit_raw = 0;
    p.high_limit_raw = FP_ONE;

    CRGB *leds = e->mCtx->leds;

    for (int i = 0; i < total_pixels; i++) {
        const fl::i32 raw_dist = mState.distance_raw[i];

        // Check distance cutoff early
        if (raw_dist > dist_cutoff_raw) {
            leds[mState.pixel_idx[i]] = CRGB(0, 0, 0);
            continue;
        }

        // dist = 0.24 * distance
        p.dist_raw = static_cast<fl::i32>(
            (static_cast<fl::i64>(dist_scale_raw) * raw_dist) >> FP::FRAC_BITS);

        // angle = 3 * polar_theta + radial[2]
        p.angle_raw = static_cast<fl::i32>(
            (static_cast<fl::i64>(three_raw) * mState.polar_theta_raw[i]) >> FP::FRAC_BITS) + radial2_raw;

        fl::i32 show1 = render_value_fp(p, fade_lut, perm);

        // Pass 2: same params but angle = 3
        p.angle_raw = angle2_raw;
        fl::i32 show2 = render_value_fp(p, fade_lut, perm);

        // dist = 1, so color = show1, (show1-show2)*0.3, (show2-show1)
        fl::i32 r = show1;
        fl::i32 g = (show1 - show2) * 3 / 10; // 0.3 factor
        fl::i32 b = show2 - show1;
        rgb_sanity_check_fp(r, g, b);

        leds[mState.pixel_idx[i]] = CRGB(
            static_cast<fl::u8>(r),
            static_cast<fl::u8>(g),
            static_cast<fl::u8>(b));
    }
}

}  // namespace fl

FL_OPTIMIZATION_LEVEL_O3_END
FL_FAST_MATH_END
