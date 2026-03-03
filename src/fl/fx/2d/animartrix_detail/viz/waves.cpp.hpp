#include "fl/compiler_control.h"
#include "fl/fx/2d/animartrix_detail/viz/waves.h"
#include "fl/fx/2d/animartrix_detail/render_value_fp.h"
#include "fl/fx/2d/animartrix_detail/perlin_float.h"

FL_FAST_MATH_BEGIN
FL_OPTIMIZATION_LEVEL_O3_BEGIN

namespace fl {

void Waves::draw(Context &ctx) {
    auto *e = ctx.mEngine.get();
    e->get_ready();

    e->timings.master_speed = 0.01;
    e->timings.ratio[0] = 2;
    e->timings.ratio[1] = 2.1;
    e->timings.ratio[2] = 1.2;

    e->timings.offset[1] = 100;
    e->timings.offset[2] = 200;
    e->timings.offset[3] = 300;

    e->calculate_oscillators(e->timings);

    for (int x = 0; x < e->num_x; x++) {
        for (int y = 0; y < e->num_y; y++) {
            e->animation.angle = e->polar_theta[x][y];
            e->animation.scale_x = 0.1;
            e->animation.scale_y = 0.1;
            e->animation.scale_z = 0.1;
            e->animation.dist = e->distance[x][y];
            e->animation.offset_y = 0;
            e->animation.offset_x = 0;
            e->animation.z = 2 * e->distance[x][y] - e->move.linear[0];
            float show1 = e->render_value(e->animation);

            e->animation.angle = e->polar_theta[x][y];
            e->animation.dist = e->distance[x][y];
            e->animation.z = 2 * e->distance[x][y] - e->move.linear[1];
            float show2 = e->render_value(e->animation);

            e->pixel.red = show1;
            e->pixel.green = 0;
            e->pixel.blue = show2;

            e->pixel = e->rgb_sanity_check(e->pixel);
            e->setPixelColorInternal(x, y, e->pixel);
        }
    }
}


// ============================================================================
// Fixed-Point Implementation of Waves
// ============================================================================

void Waves_FP::draw(Context &ctx) {
    auto *e = ctx.mEngine.get();
    e->get_ready();
    mState.ensureCache(e);
    const fl::i32 *fade_lut = fl::assume_aligned<16>(mState.fade_lut);
    const fl::u8 *perm = PERLIN_NOISE;

    e->timings.master_speed = 0.01;
    e->timings.ratio[0] = 2;
    e->timings.ratio[1] = 2.1;
    e->timings.ratio[2] = 1.2;

    e->timings.offset[1] = 100;
    e->timings.offset[2] = 200;
    e->timings.offset[3] = 300;

    e->calculate_oscillators(e->timings);

    for (int x = 0; x < e->num_x; x++) {
        for (int y = 0; y < e->num_y; y++) {
            e->animation.angle = e->polar_theta[x][y];
            e->animation.scale_x = 0.1;
            e->animation.scale_y = 0.1;
            e->animation.scale_z = 0.1;
            e->animation.dist = e->distance[x][y];
            e->animation.offset_y = 0;
            e->animation.offset_x = 0;
            e->animation.z = 2 * e->distance[x][y] - e->move.linear[0];
            float show1 = render_value_fp_from_float(e->animation, fade_lut, perm);

            e->animation.angle = e->polar_theta[x][y];
            e->animation.dist = e->distance[x][y];
            e->animation.z = 2 * e->distance[x][y] - e->move.linear[1];
            float show2 = render_value_fp_from_float(e->animation, fade_lut, perm);

            e->pixel.red = show1;
            e->pixel.green = 0;
            e->pixel.blue = show2;

            e->pixel = e->rgb_sanity_check(e->pixel);
            e->setPixelColorInternal(x, y, e->pixel);
        }
    }
}

}  // namespace fl

FL_OPTIMIZATION_LEVEL_O3_END
FL_FAST_MATH_END
