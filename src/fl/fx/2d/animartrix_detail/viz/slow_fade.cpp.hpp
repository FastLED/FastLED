#include "fl/compiler_control.h"
#include "fl/fx/2d/animartrix_detail/viz/slow_fade.h"
#include "fl/fx/2d/animartrix_detail/render_value_fp.h"
#include "fl/fx/2d/animartrix_detail/perlin_float.h"

FL_FAST_MATH_BEGIN
FL_OPTIMIZATION_LEVEL_O3_BEGIN

namespace fl {

void Slow_Fade::draw(Context &ctx) {
    auto *e = ctx.mEngine.get();
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


// ============================================================================
// Fixed-Point Implementation of Slow_Fade
// ============================================================================

void Slow_Fade_FP::draw(Context &ctx) {
    auto *e = ctx.mEngine.get();
    e->get_ready();
    mState.ensureCache(e);
    const fl::i32 *fade_lut = fl::assume_aligned<16>(mState.fade_lut);
    const fl::u8 *perm = PERLIN_NOISE;

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
            float show1 = render_value_fp_from_float(e->animation, fade_lut, perm);

            e->animation.dist = e->animation.dist * 1.1;
            e->animation.angle += e->move.noise_angle[0] / 10;
            float show2 = render_value_fp_from_float(e->animation, fade_lut, perm);

            e->animation.dist = e->animation.dist * 1.1;
            e->animation.angle += e->move.noise_angle[1] / 10;
            float show3 = render_value_fp_from_float(e->animation, fade_lut, perm);

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

}  // namespace fl

FL_OPTIMIZATION_LEVEL_O3_END
FL_FAST_MATH_END
