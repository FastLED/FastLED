#pragma once

// Generic fixed-point render_value_fp() — mirrors float render_value() in engine_core.h.
// Converts polar coordinates to cartesian, applies Perlin noise, maps to [0, 255].
// Uses sincos32() for trig, pnoise2d_raw() for noise.
//
// This is the core inner loop for all FP visualizer conversions.

#include "fl/stl/fixed_point/s16x16.h"
#include "fl/fx/2d/animartrix_detail/core_types.h"
#include "fl/fx/2d/animartrix_detail/perlin_s16x16.h"
#include "fl/math/sin32.h"

namespace fl {

// Parameters for render_value_fp, all as s16x16 raw values.
// Mirrors render_parameters from core_types.h.
struct render_parameters_fp {
    fl::i32 angle_raw;     // polar angle in radians (s16x16 raw)
    fl::i32 dist_raw;      // distance from center (s16x16 raw)
    fl::i32 scale_x_raw;   // x scale factor (s16x16 raw)
    fl::i32 scale_y_raw;   // y scale factor (s16x16 raw)
    fl::i32 scale_z_raw;   // z scale factor (s16x16 raw)
    fl::i32 offset_x_raw;  // x offset (s16x16 raw)
    fl::i32 offset_y_raw;  // y offset (s16x16 raw)
    fl::i32 offset_z_raw;  // z offset (s16x16 raw)
    fl::i32 z_raw;         // z value (s16x16 raw)
    fl::i32 center_x_raw;  // center x (s16x16 raw)
    fl::i32 center_y_raw;  // center y (s16x16 raw)
    fl::i32 low_limit_raw;  // noise low limit: 0 or -FP_ONE (s16x16 raw)
    fl::i32 high_limit_raw; // noise high limit: FP_ONE (s16x16 raw)
};

// Convert s16x16 radians to A24 angle format for sincos32.
// A24: 0 to 16777216 is a full circle (2*PI radians).
// Conversion: angle_a24 = angle_rad * (16777216 / (2*PI))
// = angle_rad * 2670177 (approximately)
FASTLED_FORCE_INLINE fl::u32 radiansToA24_fp(fl::i32 angle_s16x16_raw) {
    constexpr fl::i32 RAD_TO_A24 = 2670177; // 16777216 / (2*PI) in s16x16 sense
    return static_cast<fl::u32>(
        (static_cast<fl::i64>(angle_s16x16_raw) * RAD_TO_A24) >> fl::s16x16::FRAC_BITS);
}

// Fixed-point render_value: same algorithm as float render_value() in engine_core.h.
// Returns clamped noise value in [0, 255] range.
//
// Algorithm:
//   newx = (offset_x + center_x - cos(angle) * dist) * scale_x
//   newy = (offset_y + center_y - sin(angle) * dist) * scale_y
//   noise = pnoise2d(newx, newy)
//   clamp noise to [low_limit, high_limit]
//   map to [0, 255]
FASTLED_FORCE_INLINE fl::i32 render_value_fp(
        const render_parameters_fp &p,
        const fl::i32 *fade_lut,
        const fl::u8 *perm) {

    using FP = fl::s16x16;
    constexpr fl::i32 FP_ONE = static_cast<fl::i32>(1) << FP::FRAC_BITS;

    // sincos32 for angle
    fl::u32 a24 = radiansToA24_fp(p.angle_raw);
    SinCos32 sc = sincos32(a24);

    // sincos32 output is in [-2147418112, 2147418112] (Q0.31 range).
    // To multiply with s16x16 dist, we use:
    //   result_s16x16 = (sc_val * dist_raw) >> 31
    // This gives us the product in s16x16 format.
    fl::i32 cos_dist = static_cast<fl::i32>(
        (static_cast<fl::i64>(sc.cos_val) * p.dist_raw) >> 31);
    fl::i32 sin_dist = static_cast<fl::i32>(
        (static_cast<fl::i64>(sc.sin_val) * p.dist_raw) >> 31);

    // newx = (offset_x + center_x - cos*dist) * scale_x
    // newy = (offset_y + center_y - sin*dist) * scale_y
    fl::i32 pre_x = p.offset_x_raw + p.center_x_raw - cos_dist;
    fl::i32 pre_y = p.offset_y_raw + p.center_y_raw - sin_dist;

    fl::i32 nx = static_cast<fl::i32>(
        (static_cast<fl::i64>(pre_x) * p.scale_x_raw) >> FP::FRAC_BITS);
    fl::i32 ny = static_cast<fl::i32>(
        (static_cast<fl::i64>(pre_y) * p.scale_y_raw) >> FP::FRAC_BITS);

    // Compute z coordinate: newz = (offset_z + z) * scale_z
    fl::i32 nz = static_cast<fl::i32>(
        (static_cast<fl::i64>(p.offset_z_raw + p.z_raw) * p.scale_z_raw) >> FP::FRAC_BITS);

    // Perlin noise: 2D when z==0, 3D otherwise
    fl::i32 raw_noise;
    if (nz == 0) {
        raw_noise = perlin_s16x16::pnoise2d_raw(nx, ny, fade_lut, perm);
    } else {
        raw_noise = perlin_s16x16::pnoise3d_raw(nx, ny, nz, fade_lut, perm);
    }

    // Clamp to [low_limit, high_limit]
    if (raw_noise < p.low_limit_raw) raw_noise = p.low_limit_raw;
    if (raw_noise > p.high_limit_raw) raw_noise = p.high_limit_raw;

    // Map from [low_limit, high_limit] to [0, 255]
    // map_float(x, low, high, 0, 255) = (x - low) * 255 / (high - low)
    fl::i32 range = p.high_limit_raw - p.low_limit_raw;
    fl::i32 shifted = raw_noise - p.low_limit_raw;

    // shifted is in [0, range], we want (shifted * 255) / range
    // For the common case where low=0, high=1: range = FP_ONE
    //   result = (shifted * 255) >> FRAC_BITS  (since shifted is in [0, FP_ONE])
    // For low=-1, high=1: range = 2*FP_ONE
    //   result = (shifted * 255) / (2*FP_ONE)
    fl::i32 result;
    if (range == FP_ONE) {
        // Optimized path: low_limit=0, high_limit=1
        result = static_cast<fl::i32>(
            (static_cast<fl::i64>(shifted) * 255) >> FP::FRAC_BITS);
    } else if (range == 2 * FP_ONE) {
        // Optimized path: low_limit=-1, high_limit=1
        result = static_cast<fl::i32>(
            (static_cast<fl::i64>(shifted) * 255) >> (FP::FRAC_BITS + 1));
    } else {
        // General path
        result = static_cast<fl::i32>(
            (static_cast<fl::i64>(shifted) * 255) / range);
    }

    // Final clamp to [0, 255]
    if (result < 0) result = 0;
    if (result > 255) result = 255;

    return result;
}

// Color blend functions operating on [0, 255] integer values.
// These mirror the float versions in engine_core.h.

FASTLED_FORCE_INLINE fl::i32 multiply_fp(fl::i32 a, fl::i32 b) {
    return (a * b) / 255;
}

FASTLED_FORCE_INLINE fl::i32 screen_fp(fl::i32 a, fl::i32 b) {
    // screen(a,b) = 1 - (1-a/255)*(1-b/255) * 255
    // = 255 - (255-a)*(255-b)/255
    return 255 - ((255 - a) * (255 - b)) / 255;
}

FASTLED_FORCE_INLINE fl::i32 colordodge_fp(fl::i32 a, fl::i32 b) {
    if (b >= 255) return 255;
    fl::i32 result = (a * 255) / (255 - b);
    return result > 255 ? 255 : result;
}

FASTLED_FORCE_INLINE fl::i32 colorburn_fp(fl::i32 a, fl::i32 b) {
    if (b <= 0) return 0;
    fl::i32 result = 255 - ((255 - a) * 255) / b;
    return result < 0 ? 0 : result;
}

FASTLED_FORCE_INLINE fl::i32 subtract_fp(fl::i32 a, fl::i32 b) {
    return a - b;
}

FASTLED_FORCE_INLINE fl::i32 add_fp(fl::i32 a, fl::i32 b) {
    return a + b;
}

// Clamp RGB triple to [0, 255]
FASTLED_FORCE_INLINE void rgb_sanity_check_fp(fl::i32 &r, fl::i32 &g, fl::i32 &b) {
    if (r < 0) r = 0;
    if (r > 255) r = 255;
    if (g < 0) g = 0;
    if (g > 255) g = 255;
    if (b < 0) b = 0;
    if (b > 255) b = 255;
}

// Hybrid helper: takes float render_parameters (same struct the float path uses),
// converts to FP on-the-fly, and calls render_value_fp().
// This enables a trivial conversion pattern: replace e->render_value(e->animation)
// with render_value_fp_from_float(e->animation, fade_lut, perm).
// The FP speedup comes from sincos32 + pnoise2d_raw replacing sinf/cosf + float pnoise.
FASTLED_FORCE_INLINE fl::i32 render_value_fp_from_float(
        const render_parameters &anim,
        const fl::i32 *fade_lut,
        const fl::u8 *perm) {
    using FP = fl::s16x16;
    render_parameters_fp p;
    p.angle_raw = FP(anim.angle).raw();
    p.dist_raw = FP(anim.dist).raw();
    p.scale_x_raw = FP(anim.scale_x).raw();
    p.scale_y_raw = FP(anim.scale_y).raw();
    p.scale_z_raw = FP(anim.scale_z).raw();
    p.offset_x_raw = FP(anim.offset_x).raw();
    p.offset_y_raw = FP(anim.offset_y).raw();
    p.offset_z_raw = FP(anim.offset_z).raw();
    p.z_raw = FP(anim.z).raw();
    p.center_x_raw = FP(anim.center_x).raw();
    p.center_y_raw = FP(anim.center_y).raw();
    p.low_limit_raw = FP(anim.low_limit).raw();
    p.high_limit_raw = FP(anim.high_limit).raw();
    return render_value_fp(p, fade_lut, perm);
}

}  // namespace fl
