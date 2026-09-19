/// @file five_bit_hd_gamma.h
/// Declares functions for five-bit gamma correction

#pragma once

#include "fl/stl/int.h"
#include "fl/stl/span.h"
#include "fl/stl/noexcept.h"

namespace fl {

// Packed gamma-corrected pixel: 3 bytes RGB + 1 byte 5-bit brightness.
// 4 bytes total, cache-friendly for bulk output.
struct CRGBA5 {
    CRGB color;
    u8 brightness_5bit;
};

enum class FiveBitGammaCorrectionMode {
    kFiveBitGammaCorrectionMode_Null = 0,
    kFiveBitGammaCorrectionMode_BitShift = 1
};

// Applies five-bit HD gamma correction over a span of pixels.
// colors_scale and global_brightness are uniform across all pixels.
// Two-span output variant (separate color and brightness arrays).
void five_bit_hd_gamma_bitshift(
    fl::span<const CRGB> colors, CRGB colors_scale, fl::u8 global_brightness,
    fl::span<CRGB> out_colors, fl::span<fl::u8> out_power_5bit) FL_NO_EXCEPT;

// Packed CRGBA5 output variant (cache-friendly single array).
void five_bit_hd_gamma_bitshift(
    fl::span<const CRGB> colors, CRGB colors_scale, fl::u8 global_brightness,
    fl::span<CRGBA5> out) FL_NO_EXCEPT;

// A joint code/field solve on one pixel of 16-bit *linear* drives -- what a
// colour-managed channel already holds, so no gamma and no brightness are
// applied here (the pipeline did both). Picks the smallest 5-bit field that
// carries the brightest channel, no lower than `min_field` -- B1's flicker
// floor for APA102's slow-PWM field (#4042) -- and rounds the codes at it.
// Emitted light (code x field) never falls as a drive rises. `min_field` 31
// pins the field, as for SK9822.
void five_bit_hd_solve16(fl::u16 r16, fl::u16 g16, fl::u16 b16,
                         fl::u8 min_field, CRGB* out,
                         fl::u8* out_field) FL_NO_EXCEPT;

} // namespace fl
