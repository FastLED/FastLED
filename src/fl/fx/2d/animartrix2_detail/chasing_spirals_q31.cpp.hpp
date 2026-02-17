// Chasing_Spirals Q31 scalar implementation (fixed-point, non-vectorized)

#include "fl/align.h"  // Required for FL_ALIGNAS
#include "fl/fx/2d/animartrix2_detail/engine.h"
#include "fl/sin32.h"
#include "chasing_spirals_q31.h"

namespace fl {

namespace {
    using FP = fl::s16x16;
    using Perlin = perlin_s16x16;
    using PixelLUT = ChasingSpiralPixelLUT;
}

void Chasing_Spirals_Q31(Context &ctx) {
    // Common frame setup: timing, constants, LUTs
    auto setup = setupChasingSpiralFrame(ctx);
    const int total_pixels = setup.total_pixels;
    const PixelLUT *lut = setup.lut;
    const i32 *fade_lut = setup.fade_lut;
    const u8 *perm = setup.perm;
    const i32 cx_raw = setup.cx_raw;
    const i32 cy_raw = setup.cy_raw;
    const i32 lin0_raw = setup.lin0_raw;
    const i32 lin1_raw = setup.lin1_raw;
    const i32 lin2_raw = setup.lin2_raw;
    const i32 rad0_raw = setup.rad0_raw;
    const i32 rad1_raw = setup.rad1_raw;
    const i32 rad2_raw = setup.rad2_raw;
    CRGB *leds = setup.leds;

    constexpr i32 FP_ONE = 1 << FP::FRAC_BITS;

    // Computes one noise channel: sincos → cartesian → Perlin → clamp → scale.
    // Uses full sin32/cos32 precision (31-bit) for the coordinate computation
    // to reduce truncation error vs converting to s16x16 first.
    // Returns s16x16 raw value representing [0, 255].
    //
    // Precision analysis:
    //   sin32/cos32 output: 31-bit signed (range ±2^31, effectively ±1.0)
    //   dist_raw: s16x16 (16 frac bits, typically small values 0..~22)
    //   Product: (sin32_val * dist_raw) uses i64, shift by 31 → s16x16 format
    //   This preserves 15 more bits than the s16x16 sincos path.
    constexpr i32 RAD_TO_A24 = 2670177; // 256/(2*PI) in s16x16

    auto noise_channel = [&](i32 base_raw, i32 rad_raw,
                             i32 lin_raw, i32 dist_raw, int /*pixel_idx*/) -> i32 {
        // Convert angle (s16x16 radians) to sin32/cos32 input format
        u32 a24 = static_cast<u32>(
            (static_cast<i64>(base_raw + rad_raw) * RAD_TO_A24) >> FP::FRAC_BITS);
        SinCos32 sc = sincos32(a24);
        // Coordinate computation with 31-bit trig precision
        i32 nx = lin_raw + cx_raw -
            static_cast<i32>((static_cast<i64>(sc.cos_val) * dist_raw) >> 31);
        i32 ny = cy_raw -
            static_cast<i32>((static_cast<i64>(sc.sin_val) * dist_raw) >> 31);

        i32 raw = Perlin::pnoise2d_raw(nx, ny, fade_lut, perm);
        if (raw < 0) raw = 0;
        if (raw > FP_ONE) raw = FP_ONE;
        return raw * 255;
    };

    for (int i = 0; i < total_pixels; i++) {
        const PixelLUT &px = lut[i];
        const i32 base_raw = px.base_angle.raw();
        const i32 dist_raw = px.dist_scaled.raw();

        // Three noise channels (explicitly unrolled)
        i32 s0 = noise_channel(base_raw, rad0_raw, lin0_raw, dist_raw, i);
        i32 s1 = noise_channel(base_raw, rad1_raw, lin1_raw, dist_raw, i);
        i32 s2 = noise_channel(base_raw, rad2_raw, lin2_raw, dist_raw, i);

        // Apply radial filter: (show * rf) >> 32 gives final u8 value.
        i32 r = static_cast<i32>((static_cast<i64>(s0) * px.rf3.raw()) >> (FP::FRAC_BITS * 2));
        i32 g = static_cast<i32>((static_cast<i64>(s1) * px.rf_half.raw()) >> (FP::FRAC_BITS * 2));
        i32 b = static_cast<i32>((static_cast<i64>(s2) * px.rf_quarter.raw()) >> (FP::FRAC_BITS * 2));

        if (r < 0) r = 0; if (r > 255) r = 255;
        if (g < 0) g = 0; if (g > 255) g = 255;
        if (b < 0) b = 0; if (b > 255) b = 255;

        leds[px.pixel_idx] = CRGB(static_cast<u8>(r),
                                   static_cast<u8>(g),
                                   static_cast<u8>(b));
    }
}

} // namespace fl
