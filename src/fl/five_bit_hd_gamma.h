/// @file five_bit_hd_gamma.h
/// Declares functions for five-bit gamma correction

#pragma once

#include "fl/int.h"

#include "crgb.h"

namespace fl {

enum FiveBitGammaCorrectionMode {
    kFiveBitGammaCorrectionMode_Null = 0,
    kFiveBitGammaCorrectionMode_BitShift = 1
};

// Applies gamma correction for the RGBV(8, 8, 8, 5) color space, where
// the last byte is the brightness byte at 5 bits.

// Exposed for testing.
void five_bit_bitshift(u16 r16, u16 g16, u16 b16, fl::u8 brightness, CRGB *out,
                       fl::u8 *out_power_5bit);

void five_bit_hd_gamma_bitshift(
    CRGB colors, CRGB colors_scale, fl::u8 global_brightness, CRGB *out_colors,
    fl::u8 *out_power_5bit);

} // namespace fl
