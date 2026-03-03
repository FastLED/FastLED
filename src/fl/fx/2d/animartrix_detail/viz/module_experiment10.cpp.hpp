#include "fl/compiler_control.h"
#include "fl/fx/2d/animartrix_detail/viz/module_experiment10.h"
#include "fl/fx/2d/animartrix_detail/render_value_fp.h"
#include "fl/fx/2d/animartrix_detail/perlin_float.h"

FL_FAST_MATH_BEGIN
FL_OPTIMIZATION_LEVEL_O3_BEGIN

namespace fl {

void Module_Experiment10::draw(Context &ctx) {
    auto *e = ctx.mEngine.get();
    e->get_ready();

    e->timings.master_speed = 0.01;

    float w = 1;

    e->timings.ratio[0] = 0.01;
    e->timings.ratio[1] = 0.011;
    e->timings.ratio[2] = 0.013;
    e->timings.ratio[3] = 0.33 * w;
    e->timings.ratio[4] = 0.36 * w;
    e->timings.ratio[5] = 0.38 * w;
    e->timings.ratio[6] = 0.0003;

    e->timings.offset[0] = 0;
    e->timings.offset[1] = 100;
    e->timings.offset[2] = 200;
    e->timings.offset[3] = 300;
    e->timings.offset[4] = 400;
    e->timings.offset[5] = 500;
    e->timings.offset[6] = 600;

    e->calculate_oscillators(e->timings);

    for (int x = 0; x < e->num_x; x++) {
        for (int y = 0; y < e->num_y; y++) {

            float s = 0.4;
            float r = 1.5;

            e->animation.dist =
                3 + e->distance[x][y] +
                3 * fl::sinf(0.25 * e->distance[x][y] - e->move.radial[3]);
            e->animation.angle = e->polar_theta[x][y] + e->move.noise_angle[0] +
                                 e->move.noise_angle[6];
            e->animation.z = 5;
            e->animation.scale_x = 0.1 * s;
            e->animation.scale_y = 0.1 * s;
            e->animation.offset_z = 10 * e->move.linear[0];
            e->animation.offset_y = -5 * r * e->move.linear[0];
            e->animation.offset_x = 10;
            e->animation.low_limit = 0;
            e->show1 = e->render_value(e->animation);

            e->animation.dist =
                4 + e->distance[x][y] +
                4 * fl::sinf(0.24 * e->distance[x][y] - e->move.radial[4]);
            e->animation.angle = e->polar_theta[x][y] + e->move.noise_angle[1] +
                                 e->move.noise_angle[6];
            e->animation.z = 5;
            e->animation.scale_x = 0.1 * s;
            e->animation.scale_y = 0.1 * s;
            e->animation.offset_z = 0.1 * e->move.linear[1];
            e->animation.offset_y = -5 * r * e->move.linear[1];
            e->animation.offset_x = 100;
            e->animation.low_limit = 0;
            e->show2 = e->render_value(e->animation);

            e->animation.dist =
                5 + e->distance[x][y] +
                5 * fl::sinf(0.23 * e->distance[x][y] - e->move.radial[5]);
            e->animation.angle = e->polar_theta[x][y] + e->move.noise_angle[2] +
                                 e->move.noise_angle[6];
            e->animation.z = 5;
            e->animation.scale_x = 0.1 * s;
            e->animation.scale_y = 0.1 * s;
            e->animation.offset_z = 0.1 * e->move.linear[2];
            e->animation.offset_y = -5 * r * e->move.linear[2];
            e->animation.offset_x = 1000;
            e->animation.low_limit = 0;
            e->show3 = e->render_value(e->animation);

            e->show4 = e->colordodge(e->show1, e->show2);

            float rad = fl::sinf(PI / 2 +
                             e->distance[x][y] / 14);

            CHSV(rad * ((e->show1 + e->show2) + e->show3), 255, 255);

            e->pixel = e->rgb_sanity_check(e->pixel);

            fl::u8 a = e->getTime() / 100;
            CRGB p = CRGB(CHSV(((a + e->show1 + e->show2) + e->show3), 255, 255));
            rgb pixel;
            pixel.red = p.red;
            pixel.green = p.green;
            pixel.blue = p.blue;
            e->setPixelColorInternal(x, y, pixel);
        }
    }
}


// ============================================================================
// Fixed-Point Implementation of Module_Experiment10
// ============================================================================

void Module_Experiment10_FP::draw(Context &ctx) {
    using FP = fl::s16x16;

    auto *e = ctx.mEngine.get();
    e->get_ready();
    mState.ensureCache(e);
    const fl::i32 *fade_lut = fl::assume_aligned<16>(mState.fade_lut);
    const fl::u8 *perm = PERLIN_NOISE;

    e->timings.master_speed = 0.01;

    float w = 1;

    e->timings.ratio[0] = 0.01;
    e->timings.ratio[1] = 0.011;
    e->timings.ratio[2] = 0.013;
    e->timings.ratio[3] = 0.33 * w;
    e->timings.ratio[4] = 0.36 * w;
    e->timings.ratio[5] = 0.38 * w;
    e->timings.ratio[6] = 0.0003;

    e->timings.offset[0] = 0;
    e->timings.offset[1] = 100;
    e->timings.offset[2] = 200;
    e->timings.offset[3] = 300;
    e->timings.offset[4] = 400;
    e->timings.offset[5] = 500;
    e->timings.offset[6] = 600;

    e->calculate_oscillators(e->timings);

    const int total_pixels = mState.count;
    constexpr fl::i32 FP_ONE = static_cast<fl::i32>(1) << FP::FRAC_BITS;
    constexpr float r_factor = 1.5f;

    const fl::i32 cx_raw = FP(e->animation.center_x).raw();
    const fl::i32 cy_raw = FP(e->animation.center_y).raw();
    const fl::i32 scale_xy_raw = FP(0.1f * 0.4f).raw(); // 0.1 * s

    // Per-frame angle offsets for each pass
    const fl::i32 angle_offset1_raw = FP(e->move.noise_angle[0] + e->move.noise_angle[6]).raw();
    const fl::i32 angle_offset2_raw = FP(e->move.noise_angle[1] + e->move.noise_angle[6]).raw();
    const fl::i32 angle_offset3_raw = FP(e->move.noise_angle[2] + e->move.noise_angle[6]).raw();

    // Per-frame sinf arguments
    const float radial3 = e->move.radial[3];
    const float radial4 = e->move.radial[4];
    const float radial5 = e->move.radial[5];

    // Per-frame time hue offset
    const fl::u8 a = e->getTime() / 100;

    // Base params struct (shared across all 3 passes)
    render_parameters_fp p = {};
    p.scale_x_raw = scale_xy_raw;
    p.scale_y_raw = scale_xy_raw;
    p.scale_z_raw = 0;
    p.z_raw = FP(5.0f).raw();
    p.center_x_raw = cx_raw;
    p.center_y_raw = cy_raw;
    p.low_limit_raw = 0;
    p.high_limit_raw = FP_ONE;

    // Pass-specific per-frame constants
    const fl::i32 oz1_raw = FP(10.0f * e->move.linear[0]).raw();
    const fl::i32 oy1_raw = FP(-5.0f * r_factor * e->move.linear[0]).raw();
    const fl::i32 ox1_raw = FP(10.0f).raw();

    const fl::i32 oz2_raw = FP(0.1f * e->move.linear[1]).raw();
    const fl::i32 oy2_raw = FP(-5.0f * r_factor * e->move.linear[1]).raw();
    const fl::i32 ox2_raw = FP(100.0f).raw();

    const fl::i32 oz3_raw = FP(0.1f * e->move.linear[2]).raw();
    const fl::i32 oy3_raw = FP(-5.0f * r_factor * e->move.linear[2]).raw();
    const fl::i32 ox3_raw = FP(1000.0f).raw();

    CRGB *leds = e->mCtx->leds;

    for (int i = 0; i < total_pixels; i++) {
        const fl::i32 theta_raw = mState.polar_theta_raw[i];
        const fl::i32 dist_raw_i = mState.distance_raw[i];
        // Convert distance to float once for the sinf calls
        const float dist_f = FP::from_raw(dist_raw_i).to_float();

        // Pass 1: dist = 3 + distance + 3*sin(0.25*distance - radial[3])
        fl::i32 dist1_raw = FP(3.0f + dist_f + 3.0f * fl::sinf(0.25f * dist_f - radial3)).raw();
        p.angle_raw = theta_raw + angle_offset1_raw;
        p.dist_raw = dist1_raw;
        p.offset_z_raw = oz1_raw;
        p.offset_y_raw = oy1_raw;
        p.offset_x_raw = ox1_raw;
        fl::i32 show1 = render_value_fp(p, fade_lut, perm);

        // Pass 2: dist = 4 + distance + 4*sin(0.24*distance - radial[4])
        fl::i32 dist2_raw = FP(4.0f + dist_f + 4.0f * fl::sinf(0.24f * dist_f - radial4)).raw();
        p.angle_raw = theta_raw + angle_offset2_raw;
        p.dist_raw = dist2_raw;
        p.offset_z_raw = oz2_raw;
        p.offset_y_raw = oy2_raw;
        p.offset_x_raw = ox2_raw;
        fl::i32 show2 = render_value_fp(p, fade_lut, perm);

        // Pass 3: dist = 5 + distance + 5*sin(0.23*distance - radial[5])
        fl::i32 dist3_raw = FP(5.0f + dist_f + 5.0f * fl::sinf(0.23f * dist_f - radial5)).raw();
        p.angle_raw = theta_raw + angle_offset3_raw;
        p.dist_raw = dist3_raw;
        p.offset_z_raw = oz3_raw;
        p.offset_y_raw = oy3_raw;
        p.offset_x_raw = ox3_raw;
        fl::i32 show3 = render_value_fp(p, fade_lut, perm);

        // Color: CHSV with hue = (a + show1 + show2) + show3
        CRGB c = CRGB(CHSV(static_cast<fl::u8>((a + show1 + show2) + show3), 255, 255));
        leds[mState.pixel_idx[i]] = c;
    }
}

}  // namespace fl

FL_OPTIMIZATION_LEVEL_O3_END
FL_FAST_MATH_END
