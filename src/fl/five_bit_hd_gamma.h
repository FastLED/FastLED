/// @file five_bit_hd_gamma.h
/// Declares functions for five-bit gamma correction

#pragma once

#include "fl/gamma.h"
#include "fl/int.h"
#include "fl/math.h"

#include "crgb.h"
#include "lib8tion/scale8.h"

namespace fl {

enum FiveBitGammaCorrectionMode {
    kFiveBitGammaCorrectionMode_Null = 0,
    kFiveBitGammaCorrectionMode_BitShift = 1
};

// Applies gamma correction for the RGBV(8, 8, 8, 5) color space, where
// the last byte is the brightness byte at 5 bits.
// To override this five_bit_hd_gamma_bitshift you'll need to define
// FASTLED_FIVE_BIT_HD_BITSHIFT_FUNCTION_OVERRIDE in your build settings
// then define the function anywhere in your project.
// Example:
//  FASTLED_NAMESPACE_BEGIN
//  void five_bit_hd_gamma_bitshift(
//      fl::u8 r8, fl::u8 g8, fl::u8 b8,
//      fl::u8 r8_scale, fl::u8 g8_scale, fl::u8 b8_scale,
//      fl::u8* out_r8,
//      fl::u8* out_g8,
//      fl::u8* out_b8,
//      fl::u8* out_power_5bit) {
//        cout << "hello world\n";
//  }
//  FASTLED_NAMESPACE_END

// Force push

void internal_builtin_five_bit_hd_gamma_bitshift(CRGB colors, CRGB colors_scale,
                                                 fl::u8 global_brightness,
                                                 CRGB *out_colors,
                                                 fl::u8 *out_power_5bit);

// Exposed for testing.
void five_bit_bitshift(u16 r16, u16 g16, u16 b16, fl::u8 brightness, CRGB *out,
                       fl::u8 *out_power_5bit);

#ifdef FASTLED_FIVE_BIT_HD_BITSHIFT_FUNCTION_OVERRIDE
// This function is located somewhere else in your project, so it's declared
// extern here.
extern void five_bit_hd_gamma_bitshift(CRGB colors, CRGB colors_scale,
                                       fl::u8 global_brightness,
                                       CRGB *out_colors,
                                       fl::u8 *out_power_5bit);
#else
inline void five_bit_hd_gamma_bitshift(CRGB colors, CRGB colors_scale,
                                       fl::u8 global_brightness,
                                       CRGB *out_colors,
                                       fl::u8 *out_power_5bit) {
    internal_builtin_five_bit_hd_gamma_bitshift(
        colors, colors_scale, global_brightness, out_colors, out_power_5bit);
}
#endif // FASTLED_FIVE_BIT_HD_BITSHIFT_FUNCTION_OVERRIDE

// Simple gamma correction function that converts from
// 8-bit color component and converts it to gamma corrected 16-bit
// color component. Fast and no memory overhead!
// To override this function you'll need to define
// FASTLED_FIVE_BIT_HD_GAMMA_BITSHIFT_FUNCTION_OVERRIDE in your build settings
// and then define your own version anywhere in your project. Example:
//  FASTLED_NAMESPACE_BEGIN
//  void five_bit_hd_gamma_function(
//    fl::u8 r8, fl::u8 g8, fl::u8 b8,
//    u16* r16, u16* g16, u16* b16) {
//      cout << "hello world\n";
//  }
//  FASTLED_NAMESPACE_END
#ifdef FASTLED_FIVE_BIT_HD_GAMMA_FUNCTION_OVERRIDE
// This function is located somewhere else in your project, so it's declared
// extern here.
extern void five_bit_hd_gamma_function(CRGB color, u16 *r16, u16 *g16,
                                       u16 *b16);
#else
inline void five_bit_hd_gamma_function(CRGB color, u16 *r16, u16 *g16,
                                       u16 *b16) {

    gamma16(color, r16, g16, b16);
}
#endif // FASTLED_FIVE_BIT_HD_GAMMA_FUNCTION_OVERRIDE

inline void internal_builtin_five_bit_hd_gamma_bitshift(
    CRGB colors, CRGB colors_scale, fl::u8 global_brightness, CRGB *out_colors,
    fl::u8 *out_power_5bit) {

    if (global_brightness == 0) {
        *out_colors = CRGB(0, 0, 0);
        *out_power_5bit = 0;
        return;
    }

    // Step 1: Gamma Correction
    u16 r16, g16, b16;
    five_bit_hd_gamma_function(colors, &r16, &g16, &b16);

    // Step 2: Color correction step comes after gamma correction. These values
    // are assumed to be be relatively close to 255.
    if (colors_scale.r != 0xff) {
        r16 = scale16by8(r16, colors_scale.r);
    }
    if (colors_scale.g != 0xff) {
        g16 = scale16by8(g16, colors_scale.g);
    }
    if (colors_scale.b != 0xff) {
        b16 = scale16by8(b16, colors_scale.b);
    }

    five_bit_bitshift(r16, g16, b16, global_brightness, out_colors,
                      out_power_5bit);
}

// Since the return value wasn't used, it has been omitted.
// It's not clear what scale brightness is, or how it is to be applied,
// so we assume 8 bits applied over the given rgb values.
inline void five_bit_bitshift(uint16_t r16, uint16_t g16, uint16_t b16,
                              uint8_t brightness, CRGB *out,
                              uint8_t *out_power_5bit) {

    // NEW in 3.10.2: A new closed form solution has been found!
    // Thank you https://github.com/gwgill!
    // It's okay if you don't know how this works, few do, but it tests
    // very well and is better than the old iterative approach which had
    // bad quantization issues (sudden jumps in brightness in certain intervals).

    // ix/31 * 255/65536 * 256 scaling factors, valid for indexes 1..31
    static uint32_t bright_scale[32] = {
        0,      2023680, 1011840, 674560, 505920, 404736, 337280, 289097,
        252960, 224853,  202368,  183971, 168640, 155668, 144549, 134912,
        126480, 119040,  112427,  106509, 101184, 96366,  91985,  87986,
        84320,  80947,   77834,   74951,  72274,  69782,  67456,  65280};

    auto max3 = [](u16 a, u16 b, u16 c) { return fl_max(fl_max(a, b), c); };


    if (brightness == 0) {
        *out = CRGB(0, 0, 0);
        *out_power_5bit = 0;
        return;
    }
    if (r16 == 0 && g16 == 0 && b16 == 0) {
        *out = CRGB(0, 0, 0);
        *out_power_5bit = (brightness <= 31) ? brightness : 31;
        return;
    }

    uint8_t r8 = 0, g8 = 0, b8 = 0;

    // Apply any brightness setting (we assume brightness is 0..255)
    if (brightness != 0xff) {
        r16 = scale16by8(r16, brightness);
        g16 = scale16by8(g16, brightness);
        b16 = scale16by8(b16, brightness);
    }

    // Locate the largest value to set the brightness/scale factor
    uint16_t scale = max3(r16, g16, b16);

    if (scale == 0) {
        *out = CRGB(0, 0, 0);
        *out_power_5bit = 0;
        return;
    } else {
        uint32_t scalef;

        // Compute 5 bit quantized scale that is at or above the maximum value.
        scale = (scale + (2047 - (scale >> 5))) >> 11;

        // Adjust the 16 bit values to account for the scale, then round to 8
        // bits
        scalef = bright_scale[scale];
        r8 = (r16 * scalef + 0x808000) >> 24;
        g8 = (g16 * scalef + 0x808000) >> 24;
        b8 = (b16 * scalef + 0x808000) >> 24;

        *out = CRGB(r8, g8, b8);
        *out_power_5bit = static_cast<uint8_t>(scale);
        return;
    }
}

} // namespace fl
