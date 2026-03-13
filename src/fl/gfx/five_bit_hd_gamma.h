/// @file five_bit_hd_gamma.h
/// Declares functions for five-bit gamma correction

#pragma once

#include "fl/stl/int.h"
#include "fl/stl/span.h"

namespace fl {

// Packed gamma-corrected pixel: 3 bytes RGB + 1 byte 5-bit brightness.
// 4 bytes total, cache-friendly for bulk output.
struct CRGBA5 {
    CRGB color;
    u8 brightness_5bit;
};

enum FiveBitGammaCorrectionMode {
    kFiveBitGammaCorrectionMode_Null = 0,
    kFiveBitGammaCorrectionMode_BitShift = 1
};

// Applies five-bit HD gamma correction over a span of pixels.
// colors_scale and global_brightness are uniform across all pixels.
// Two-span output variant (separate color and brightness arrays).
void five_bit_hd_gamma_bitshift(
    fl::span<const CRGB> colors, CRGB colors_scale, fl::u8 global_brightness,
    fl::span<CRGB> out_colors, fl::span<fl::u8> out_power_5bit);

// Packed CRGBA5 output variant (cache-friendly single array).
void five_bit_hd_gamma_bitshift(
    fl::span<const CRGB> colors, CRGB colors_scale, fl::u8 global_brightness,
    fl::span<CRGBA5> out);

} // namespace fl
