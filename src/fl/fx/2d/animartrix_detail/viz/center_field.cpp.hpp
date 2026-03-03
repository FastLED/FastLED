#include "fl/compiler_control.h"
#include "fl/fx/2d/animartrix_detail/viz/center_field.h"
#include "fl/fx/2d/animartrix_detail/render_value_fp.h"
#include "fl/fx/2d/animartrix_detail/perlin_float.h"

FL_FAST_MATH_BEGIN
FL_OPTIMIZATION_LEVEL_O3_BEGIN

namespace fl {

void Center_Field::draw(Context &ctx) {
    auto *e = ctx.mEngine.get();
    e->get_ready();

    // Perlin noise is periodic with period 256 at integer coordinates.
    // With scale_y = 0.07, the effective period for offset_y is 256/0.07 ≈ 3657.14.
    // Reducing modulo this period keeps float32 precise at extreme uptimes.
    static constexpr float perlin_period = 3657.14285714f; // 256.0f / 0.07f

    e->timings.master_speed = 0.01;
    e->timings.ratio[0] = 1;
    e->timings.ratio[1] = 1.1;
    e->timings.ratio[2] = 1.2;

    e->timings.offset[1] = 100;
    e->timings.offset[2] = 200;
    e->timings.offset[3] = 300;

    e->calculate_oscillators(e->timings);

    for (int x = 0; x < e->num_x; x++) {
        for (int y = 0; y < e->num_y; y++) {
            e->animation.angle = e->polar_theta[x][y];
            e->animation.scale_x = 0.07;
            e->animation.scale_y = 0.07;
            e->animation.scale_z = 0.1;
            e->animation.dist = 5 * fl::sqrtf(e->distance[x][y]);
            e->animation.offset_y = fl::fmodf(e->move.linear[0], perlin_period);
            e->animation.offset_x = 0;
            e->animation.z = 0;
            float show1 = e->render_value(e->animation);

            e->animation.angle = e->polar_theta[x][y];
            e->animation.scale_x = 0.07;
            e->animation.scale_y = 0.07;
            e->animation.scale_z = 0.1;
            e->animation.dist = 4 * fl::sqrtf(e->distance[x][y]);
            e->animation.offset_y = fl::fmodf(e->move.linear[0], perlin_period);
            e->animation.offset_x = 0;
            e->animation.z = 0;
            float show2 = e->render_value(e->animation);

            e->pixel.red = show1;
            e->pixel.green = show2;
            e->pixel.blue = 0;

            e->pixel = e->rgb_sanity_check(e->pixel);
            e->setPixelColorInternal(x, y, e->pixel);
        }
    }
}

// ============================================================================
// Fixed-Point Q31 Implementation of Center_Field
// ============================================================================

void Center_Field_FP::draw(Context &ctx) {
    using FP = fl::s16x16;

    auto *e = ctx.mEngine.get();
    e->get_ready();

    // Same timing setup as float
    e->timings.master_speed = 0.01;
    e->timings.ratio[0] = 1;
    e->timings.ratio[1] = 1.1;
    e->timings.ratio[2] = 1.2;
    e->timings.offset[1] = 100;
    e->timings.offset[2] = 200;
    e->timings.offset[3] = 300;
    e->calculate_oscillators(e->timings);

    // Build/update per-pixel geometry cache
    mState.ensureCache(e);

    const int total_pixels = mState.count;
    const fl::i32 *fade_lut = fl::assume_aligned<16>(mState.fade_lut);
    const fl::u8 *perm = PERLIN_NOISE;

    // Per-frame constants converted to s16x16
    constexpr FP scale_xy(0.07f);
    const FP center_x_fp(e->animation.center_x);
    const FP center_y_fp(e->animation.center_y);

    // Period reduction: Perlin noise is periodic with period 256 at integer
    // coordinates. With scale_y = 0.07, the effective period for offset_y is
    // 256 / 0.07 ≈ 3657.14. Reducing modulo this period prevents s16x16
    // overflow and maintains precision at high time values.
    constexpr float perlin_period = 3657.14285714f; // 256.0f / 0.07f
    const FP offset_y_fp(fl::fmodf(e->move.linear[0], perlin_period));
    constexpr FP five(5.0f);
    constexpr FP four(4.0f);
    const fl::i32 scale_xy_raw = scale_xy.raw();
    const fl::i32 cx_raw = center_x_fp.raw();
    const fl::i32 cy_raw = center_y_fp.raw();
    const fl::i32 oy_raw = offset_y_fp.raw();
    const fl::i32 five_raw = five.raw();
    const fl::i32 four_raw = four.raw();

    // low_limit=0, high_limit=1 (default render_parameters)
    constexpr fl::i32 FP_ONE = static_cast<fl::i32>(1) << FP::FRAC_BITS;

    render_parameters_fp p = {};
    p.scale_x_raw = scale_xy_raw;
    p.scale_y_raw = scale_xy_raw;
    p.scale_z_raw = FP(0.1f).raw();
    p.offset_x_raw = 0;
    p.offset_y_raw = oy_raw;
    p.offset_z_raw = 0;
    p.z_raw = 0;
    p.center_x_raw = cx_raw;
    p.center_y_raw = cy_raw;
    p.low_limit_raw = 0;
    p.high_limit_raw = FP_ONE;

    CRGB *leds = e->mCtx->leds;

    for (int i = 0; i < total_pixels; i++) {
        const fl::i32 theta_raw = mState.polar_theta_raw[i];
        const fl::i32 sqrt_dist_raw = mState.sqrt_distance_raw[i];

        // Pass 1: dist = 5 * sqrt(distance)
        fl::i32 dist1_raw = static_cast<fl::i32>(
            (static_cast<fl::i64>(five_raw) * sqrt_dist_raw) >> FP::FRAC_BITS);
        p.angle_raw = theta_raw;
        p.dist_raw = dist1_raw;
        fl::i32 show1 = render_value_fp(p, fade_lut, perm);

        // Pass 2: dist = 4 * sqrt(distance)
        fl::i32 dist2_raw = static_cast<fl::i32>(
            (static_cast<fl::i64>(four_raw) * sqrt_dist_raw) >> FP::FRAC_BITS);
        p.dist_raw = dist2_raw;
        fl::i32 show2 = render_value_fp(p, fade_lut, perm);

        // pixel.red = show1, pixel.green = show2, pixel.blue = 0
        fl::i32 r = show1;
        fl::i32 g = show2;
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
