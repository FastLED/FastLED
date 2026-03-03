#include "fl/compiler_control.h"
#include "fl/fx/2d/animartrix_detail/viz/hot_blob.h"
#include "fl/fx/2d/animartrix_detail/render_value_fp.h"
#include "fl/fx/2d/animartrix_detail/perlin_float.h"

FL_FAST_MATH_BEGIN
FL_OPTIMIZATION_LEVEL_O3_BEGIN

namespace fl {

void Hot_Blob::draw(Context &ctx) {
    auto *e = ctx.mEngine.get();
    e->get_ready();
    e->run_default_oscillators(0.001);

    for (int x = 0; x < e->num_x; x++) {
        for (int y = 0; y < e->num_y; y++) {
            e->animation.dist = e->distance[x][y];
            e->animation.angle = e->polar_theta[x][y];

            e->animation.scale_x = 0.07 + e->move.directional[0] * 0.002;
            e->animation.scale_y = 0.07;

            e->animation.offset_y = -e->move.linear[0];
            e->animation.offset_x = 0;
            e->animation.offset_z = 0;

            e->animation.z = 0;
            e->animation.low_limit = -1;
            float show1 = e->render_value(e->animation);

            e->animation.offset_y = -e->move.linear[1];
            float show3 = e->render_value(e->animation);

            e->animation.offset_x = show3 / 20;
            e->animation.offset_y = -e->move.linear[0] / 2 + show1 / 70;
            e->animation.low_limit = 0;
            float show2 = e->render_value(e->animation);

            e->animation.offset_x = show3 / 20;
            e->animation.offset_y = -e->move.linear[0] / 2 + show1 / 70;
            e->animation.z = 100;
            float show4 = e->render_value(e->animation);

            float radius = e->radial_filter_radius;
            float radial = (radius - e->animation.dist) / e->animation.dist;

            float linear = (y + 1) / (e->num_y - 1.f);

            e->pixel.red = radial * show2;
            e->pixel.green = linear * radial * 0.3 * (show2 - show4);
            e->pixel.blue = 0;

            e->pixel = e->rgb_sanity_check(e->pixel);
            e->setPixelColorInternal(x, y, e->pixel);
        }
    }
}


// ============================================================================
// Fixed-Point Implementation of Hot_Blob
// ============================================================================

void Hot_Blob_FP::draw(Context &ctx) {
    auto *e = ctx.mEngine.get();
    e->get_ready();
    mState.ensureCache(e);
    const fl::i32 *fade_lut = fl::assume_aligned<16>(mState.fade_lut);
    const fl::u8 *perm = PERLIN_NOISE;
    e->run_default_oscillators(0.001);

    for (int x = 0; x < e->num_x; x++) {
        for (int y = 0; y < e->num_y; y++) {
            e->animation.dist = e->distance[x][y];
            e->animation.angle = e->polar_theta[x][y];

            e->animation.scale_x = 0.07 + e->move.directional[0] * 0.002;
            e->animation.scale_y = 0.07;

            e->animation.offset_y = -e->move.linear[0];
            e->animation.offset_x = 0;
            e->animation.offset_z = 0;

            e->animation.z = 0;
            e->animation.low_limit = -1;
            float show1 = render_value_fp_from_float(e->animation, fade_lut, perm);

            e->animation.offset_y = -e->move.linear[1];
            float show3 = render_value_fp_from_float(e->animation, fade_lut, perm);

            e->animation.offset_x = show3 / 20;
            e->animation.offset_y = -e->move.linear[0] / 2 + show1 / 70;
            e->animation.low_limit = 0;
            float show2 = render_value_fp_from_float(e->animation, fade_lut, perm);

            e->animation.offset_x = show3 / 20;
            e->animation.offset_y = -e->move.linear[0] / 2 + show1 / 70;
            e->animation.z = 100;
            float show4 = render_value_fp_from_float(e->animation, fade_lut, perm);

            float radius = e->radial_filter_radius;
            float radial = (radius - e->animation.dist) / e->animation.dist;

            float linear = (y + 1) / (e->num_y - 1.f);

            e->pixel.red = radial * show2;
            e->pixel.green = linear * radial * 0.3 * (show2 - show4);
            e->pixel.blue = 0;

            e->pixel = e->rgb_sanity_check(e->pixel);
            e->setPixelColorInternal(x, y, e->pixel);
        }
    }
}

}  // namespace fl

FL_OPTIMIZATION_LEVEL_O3_END
FL_FAST_MATH_END
