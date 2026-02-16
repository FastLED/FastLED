#pragma once

// Chasing_Spirals Q31 scalar implementation (fixed-point, non-vectorized)
//
// This is the baseline fixed-point implementation that uses scalar integer
// math instead of floating-point. Provides 2.7x speedup over float reference.
//
// DO NOT include directly - included by chasing_spirals.hpp

#ifndef ANIMARTRIX2_CHASING_SPIRALS_INTERNAL
#error "Do not include chasing_spirals_q31.hpp directly. Include animartrix2.hpp instead."
#endif

#include "chasing_spirals_common.hpp"

namespace animartrix2_detail {

inline void Chasing_Spirals_Q31(Context &ctx) {
    // Common frame setup: timing, constants, LUTs
    auto setup = setupChasingSpiralFrame(ctx);
    const int total_pixels = setup.total_pixels;
    const PixelLUT *lut = setup.lut;
    const fl::i32 *fade_lut = setup.fade_lut;
    const fl::u8 *perm = setup.perm;
    const fl::i32 cx_raw = setup.cx_raw;
    const fl::i32 cy_raw = setup.cy_raw;
    const fl::i32 lin0_raw = setup.lin0_raw;
    const fl::i32 lin1_raw = setup.lin1_raw;
    const fl::i32 lin2_raw = setup.lin2_raw;
    const fl::i32 rad0_raw = setup.rad0_raw;
    const fl::i32 rad1_raw = setup.rad1_raw;
    const fl::i32 rad2_raw = setup.rad2_raw;
    CRGB *leds = setup.leds;

    constexpr fl::i32 FP_ONE = 1 << FP::FRAC_BITS;

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
    constexpr fl::i32 RAD_TO_A24 = 2670177; // 256/(2*PI) in s16x16

    auto noise_channel = [&](fl::i32 base_raw, fl::i32 rad_raw,
                             fl::i32 lin_raw, fl::i32 dist_raw, int /*pixel_idx*/) -> fl::i32 {
        // Convert angle (s16x16 radians) to sin32/cos32 input format
        fl::u32 a24 = static_cast<fl::u32>(
            (static_cast<fl::i64>(base_raw + rad_raw) * RAD_TO_A24) >> FP::FRAC_BITS);
        fl::SinCos32 sc = fl::sincos32(a24);
        // Coordinate computation with 31-bit trig precision
        fl::i32 nx = lin_raw + cx_raw -
            static_cast<fl::i32>((static_cast<fl::i64>(sc.cos_val) * dist_raw) >> 31);
        fl::i32 ny = cy_raw -
            static_cast<fl::i32>((static_cast<fl::i64>(sc.sin_val) * dist_raw) >> 31);

        fl::i32 raw = Perlin::pnoise2d_raw(nx, ny, fade_lut, perm);
        if (raw < 0) raw = 0;
        if (raw > FP_ONE) raw = FP_ONE;
        return raw * 255;
    };

    for (int i = 0; i < total_pixels; i++) {
        const PixelLUT &px = lut[i];
        const fl::i32 base_raw = px.base_angle.raw();
        const fl::i32 dist_raw = px.dist_scaled.raw();

        // Three noise channels (explicitly unrolled)
        fl::i32 s0 = noise_channel(base_raw, rad0_raw, lin0_raw, dist_raw, i);
        fl::i32 s1 = noise_channel(base_raw, rad1_raw, lin1_raw, dist_raw, i);
        fl::i32 s2 = noise_channel(base_raw, rad2_raw, lin2_raw, dist_raw, i);

        // Apply radial filter: (show * rf) >> 32 gives final u8 value.
        fl::i32 r = static_cast<fl::i32>((static_cast<fl::i64>(s0) * px.rf3.raw()) >> (FP::FRAC_BITS * 2));
        fl::i32 g = static_cast<fl::i32>((static_cast<fl::i64>(s1) * px.rf_half.raw()) >> (FP::FRAC_BITS * 2));
        fl::i32 b = static_cast<fl::i32>((static_cast<fl::i64>(s2) * px.rf_quarter.raw()) >> (FP::FRAC_BITS * 2));

        if (r < 0) r = 0; if (r > 255) r = 255;
        if (g < 0) g = 0; if (g > 255) g = 255;
        if (b < 0) b = 0; if (b > 255) b = 255;

        leds[px.pixel_idx] = CRGB(static_cast<fl::u8>(r),
                                   static_cast<fl::u8>(g),
                                   static_cast<fl::u8>(b));
    }
}

} // namespace animartrix2_detail
