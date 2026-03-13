#include "fl/stl/compiler_control.h"
#include "fl/fx/2d/animartrix_detail/viz/module_experiment2.h"
#include "fl/fx/2d/animartrix_detail/render_value_fp.h"
#include "fl/fx/2d/animartrix_detail/perlin_float.h"

FL_FAST_MATH_BEGIN
FL_OPTIMIZATION_LEVEL_O3_BEGIN

namespace fl {

void Module_Experiment2::draw(Context &ctx) {
    auto *e = ctx.mEngine.get();
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


// ============================================================================
// Fixed-Point Implementation of Module_Experiment2
// ============================================================================

void Module_Experiment2_FP::draw(Context &ctx) {
    using FP = fl::s16x16;

    auto *e = ctx.mEngine.get();
    e->get_ready();
    mState.ensureCache(e);
    const fl::i32 *fade_lut = fl::assume_aligned<16>(mState.fade_lut);
    const fl::u8 *perm = PERLIN_NOISE;

    e->timings.master_speed = 0.02;
    e->timings.ratio[0] = 0.0025;
    e->timings.ratio[1] = 0.0027;
    e->timings.ratio[2] = 0.029;
    e->timings.ratio[3] = 0.033;

    e->calculate_oscillators(e->timings);

    const int total_pixels = mState.count;

    // Per-frame constants converted to s16x16 raw once
    constexpr fl::i32 FP_ONE = static_cast<fl::i32>(1) << FP::FRAC_BITS;
    constexpr fl::i32 scale_xy_raw = FP(0.1f).raw();
    const fl::i32 cx_raw = FP(e->animation.center_x).raw();
    const fl::i32 cy_raw = FP(e->animation.center_y).raw();

    // dist = distance[x][y] - (16 + directional[0] * 16)
    // The subtracted term is per-frame constant
    const fl::i32 dist_offset_raw = FP(16.0f + e->move.directional[0] * 16.0f).raw();

    // angle = noise_angle[0] + noise_angle[1] + polar_theta[x][y]
    // The first two are per-frame constants
    const fl::i32 angle_offset_raw = FP(e->move.noise_angle[0] + e->move.noise_angle[1]).raw();

    render_parameters_fp p = {};
    p.scale_x_raw = scale_xy_raw;
    p.scale_y_raw = scale_xy_raw;
    p.scale_z_raw = 0; // scale_z not set in float version (default 0)
    p.offset_x_raw = FP(10.0f).raw();
    p.offset_y_raw = FP(20.0f * e->move.linear[2]).raw();
    p.offset_z_raw = FP(-10.0f).raw();
    p.z_raw = FP(5.0f).raw();
    p.center_x_raw = cx_raw;
    p.center_y_raw = cy_raw;
    p.low_limit_raw = 0;
    p.high_limit_raw = FP_ONE;

    CRGB *leds = e->mCtx->leds;

    for (int i = 0; i < total_pixels; i++) {
        // Per-pixel: dist and angle
        p.dist_raw = mState.distance_raw[i] - dist_offset_raw;
        p.angle_raw = angle_offset_raw + mState.polar_theta_raw[i];

        fl::i32 show1 = render_value_fp(p, fade_lut, perm);

        fl::i32 r = show1;
        fl::i32 g = show1 - 80;
        fl::i32 b = show1 - 150;
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
