#include "fl/stl/compiler_control.h"
#include "fl/fx/2d/animartrix_detail/viz/complex_kaleido_5.h"
#include "fl/fx/2d/animartrix_detail/render_value_fp.h"
#include "fl/fx/2d/animartrix_detail/perlin_float.h"

FL_FAST_MATH_BEGIN
FL_OPTIMIZATION_LEVEL_O3_BEGIN

namespace fl {

void Complex_Kaleido_5::draw(Context &ctx) {
    auto *e = ctx.mEngine.get();
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


// ============================================================================
// Fixed-Point Implementation of Complex_Kaleido_5
// ============================================================================

void Complex_Kaleido_5_FP::draw(Context &ctx) {
    using FP = fl::s16x16;

    auto *e = ctx.mEngine.get();
    e->get_ready();
    mState.ensureCache(e);
    const fl::i32 *fade_lut = fl::assume_aligned<16>(mState.fade_lut);
    const fl::u8 *perm = PERLIN_NOISE;

    e->timings.master_speed = 0.01;
    e->timings.ratio[0] = 0.025;
    e->timings.ratio[1] = 0.027;
    e->timings.ratio[2] = 0.031;
    e->timings.ratio[3] = 0.033;
    e->timings.ratio[4] = 0.037;
    e->timings.ratio[5] = 0.0038;
    e->timings.ratio[6] = 0.041;

    e->calculate_oscillators(e->timings);

    const int total_pixels = mState.count;
    constexpr fl::i32 FP_ONE = static_cast<fl::i32>(1) << FP::FRAC_BITS;
    constexpr float size = 0.6f;

    // Per-frame constants
    const fl::i32 s_raw = FP(1.0f + e->move.directional[6] * 0.8f).raw();
    const fl::i32 angle_base_raw = FP(10.0f * e->move.radial[6]).raw();
    const fl::i32 theta_scale_raw = FP(50.0f * e->move.directional[5]).raw();
    constexpr fl::i32 one_third_raw = FP(1.0f / 3.0f).raw();
    const fl::i32 cx_raw = FP(e->animation.center_x).raw();
    const fl::i32 cy_raw = FP(e->animation.center_y).raw();

    // Radial filter (kept as float — it's post-Perlin, not critical path)
    const float radius = e->radial_filter_radius;

    render_parameters_fp p = {};
    p.scale_x_raw = FP(0.08f * size).raw();
    p.scale_y_raw = FP(0.07f * size).raw();
    p.scale_z_raw = 0;
    p.offset_x_raw = 0;
    p.offset_y_raw = 0;
    p.offset_z_raw = FP(-10.0f * e->move.linear[0]).raw();
    p.z_raw = FP(5.0f).raw();
    p.center_x_raw = cx_raw;
    p.center_y_raw = cy_raw;
    p.low_limit_raw = FP(-0.5f).raw();
    p.high_limit_raw = FP_ONE;

    CRGB *leds = e->mCtx->leds;

    for (int i = 0; i < total_pixels; i++) {
        const fl::i32 dist_raw = mState.distance_raw[i];

        // dist = distance * s
        p.dist_raw = static_cast<fl::i32>(
            (static_cast<fl::i64>(dist_raw) * s_raw) >> FP::FRAC_BITS);

        // angle = 10*radial[6] + 50*directional[5]*polar_theta - dist/3
        fl::i32 theta_term = static_cast<fl::i32>(
            (static_cast<fl::i64>(theta_scale_raw) * mState.polar_theta_raw[i]) >> FP::FRAC_BITS);
        fl::i32 dist_third = static_cast<fl::i32>(
            (static_cast<fl::i64>(p.dist_raw) * one_third_raw) >> FP::FRAC_BITS);
        p.angle_raw = angle_base_raw + theta_term - dist_third;

        fl::i32 show1 = render_value_fp(p, fade_lut, perm);

        // Radial filter (float post-processing — not in critical Perlin path)
        // distance_raw is s16x16; convert back to float for division
        float dist_f = FP::from_raw(dist_raw).to_float();
        float radial = (dist_f > 0.0f) ? (radius - dist_f) / dist_f : 0.0f;

        fl::i32 r = static_cast<fl::i32>(show1 * radial);
        fl::i32 g = 0;
        fl::i32 b = 0;
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
