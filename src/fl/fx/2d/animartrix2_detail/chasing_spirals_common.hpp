#pragma once

// Chasing_Spirals shared code - common helpers and structures
//
// Shared by all Chasing_Spirals implementations (scalar, SIMD, etc.)

namespace fl {

// Q31 fixed-point helper types (avoid polluting global namespace)
namespace {  // Anonymous namespace for file-local types
    using FP = fl::s16x16;
    using Perlin = animartrix2_detail::perlin_s16x16;
    using PixelLUT = animartrix2_detail::ChasingSpiralPixelLUT;
}

// Common setup values returned by setupChasingSpiralFrame
struct FrameSetup {
    int total_pixels;
    const PixelLUT *lut;
    const fl::i32 *fade_lut;
    const fl::u8 *perm;
    fl::i32 cx_raw;
    fl::i32 cy_raw;
    fl::i32 lin0_raw;
    fl::i32 lin1_raw;
    fl::i32 lin2_raw;
    fl::i32 rad0_raw;
    fl::i32 rad1_raw;
    fl::i32 rad2_raw;
    CRGB *leds;
};

// Convert s16x16 angle (radians) to A24 format for sincos32
fl::u32 radiansToA24(fl::i32 base_s16x16, fl::i32 offset_s16x16);

// Compute Perlin coordinates from SIMD sincos results and distances (4 pixels)
void simd4_computePerlinCoords(
    const fl::i32 cos_arr[4], const fl::i32 sin_arr[4],
    const fl::i32 dist_arr[4], fl::i32 lin_raw, fl::i32 cx_raw, fl::i32 cy_raw,
    fl::i32 nx_out[4], fl::i32 ny_out[4]);

// Clamp s16x16 value to [0, 1] and scale to [0, 255]
fl::i32 clampAndScale255(fl::i32 raw_s16x16);

// Apply radial filter to noise value and clamp to [0, 255]
fl::i32 applyRadialFilter(fl::i32 noise_255, fl::i32 rf_raw);

// Process one color channel for 4 pixels using SIMD (angle → sincos → Perlin → clamp)
void simd4_processChannel(
    const fl::i32 base_arr[4], const fl::i32 dist_arr[4],
    fl::i32 radial_offset, fl::i32 linear_offset,
    const fl::i32 *fade_lut, const fl::u8 *perm, fl::i32 cx_raw, fl::i32 cy_raw,
    fl::i32 noise_out[4]);

// Extract common frame setup logic shared by all Chasing_Spirals variants.
// Computes timing, scaled constants, builds PixelLUT, initializes fade LUT.
FrameSetup setupChasingSpiralFrame(animartrix2_detail::Context &ctx);

} // namespace fl
