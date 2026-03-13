#include "fl/stl/compiler_control.h"
#include "fl/fx/2d/animartrix_detail/viz/spiral_matrix4.h"
#include "fl/fx/2d/animartrix_detail/render_value_fp.h"
#include "fl/fx/2d/animartrix_detail/perlin_float.h"

FL_FAST_MATH_BEGIN
FL_OPTIMIZATION_LEVEL_O3_BEGIN

namespace fl {

void SpiralMatrix4::draw(Context &ctx) {
    auto *e = ctx.mEngine.get();
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


// ============================================================================
// Fixed-Point Implementation of SpiralMatrix4
// ============================================================================

void SpiralMatrix4_FP::draw(Context &ctx) {
    using FP = fl::s16x16;

    auto *e = ctx.mEngine.get();
    e->get_ready();
    mState.ensureCache(e);
    const fl::i32 *fade_lut = fl::assume_aligned<16>(mState.fade_lut);
    const fl::u8 *perm = PERLIN_NOISE;

    e->timings.master_speed = 0.02;
    e->timings.ratio[0] = 0.025;
    e->timings.ratio[1] = 0.027;
    e->timings.ratio[2] = 0.031;
    e->timings.ratio[3] = 0.0033;
    e->timings.ratio[4] = 0.0036;
    e->timings.ratio[5] = 0.0039;

    e->calculate_oscillators(e->timings);

    const int total_pixels = mState.count;

    constexpr fl::i32 FP_ONE = static_cast<fl::i32>(1) << FP::FRAC_BITS;
    constexpr fl::i32 scale_xy_raw = FP(0.09f).raw();
    const fl::i32 cx_raw = FP(e->animation.center_x).raw();
    const fl::i32 cy_raw = FP(e->animation.center_y).raw();

    // Pass 1 params (shared struct, only angle/dist vary per pixel)
    render_parameters_fp p = {};
    p.scale_x_raw = scale_xy_raw;
    p.scale_y_raw = scale_xy_raw;
    p.scale_z_raw = 0;
    p.offset_x_raw = 0;
    p.offset_y_raw = FP(-20.0f * e->move.linear[0]).raw();
    p.offset_z_raw = 0;
    p.z_raw = FP(5.0f).raw();
    p.center_x_raw = cx_raw;
    p.center_y_raw = cy_raw;
    p.low_limit_raw = 0;
    p.high_limit_raw = FP_ONE;

    // Pass 2 differs only in z and offset_y
    constexpr fl::i32 z1_raw = FP(5.0f).raw();
    const fl::i32 z2_raw = FP(500.0f).raw();
    const fl::i32 oy1_raw = FP(-20.0f * e->move.linear[0]).raw();
    const fl::i32 oy2_raw = FP(-40.0f * e->move.linear[0]).raw();

    CRGB *leds = e->mCtx->leds;

    for (int i = 0; i < total_pixels; i++) {
        const fl::i32 theta_raw = mState.polar_theta_raw[i];
        const fl::i32 dist_raw = mState.distance_raw[i];

        // Pass 1: z=5, offset_y=-20*linear[0]
        p.angle_raw = theta_raw;
        p.dist_raw = dist_raw;
        p.z_raw = z1_raw;
        p.offset_y_raw = oy1_raw;
        fl::i32 show1 = render_value_fp(p, fade_lut, perm);

        // Pass 2: z=500, offset_y=-40*linear[0]
        p.z_raw = z2_raw;
        p.offset_y_raw = oy2_raw;
        fl::i32 show2 = render_value_fp(p, fade_lut, perm);

        fl::i32 r = add_fp(show2, show1);
        fl::i32 g = 0;
        fl::i32 b = colordodge_fp(show2, show1);
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
