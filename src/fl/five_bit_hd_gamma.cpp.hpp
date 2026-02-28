#include "fl/five_bit_hd_gamma.h"
#include "fl/fastled.h"

namespace fl {

// ix/31 * 255/65536 * 256 scaling factors, valid for indexes 1..31.
// Uses flash storage on AVR/ESP, zero heap allocation on all platforms.
static constexpr u32 BRIGHT_SCALE[32] FL_PROGMEM = {
    0,      2023680, 1011840, 674560, 505920, 404736, 337280, 289097,
    252960, 224853,  202368,  183971, 168640, 155668, 144549, 134912,
    126480, 119040,  112427,  106509, 101184, 96366,  91985,  87986,
    84320,  80947,   77834,   74951,  72274,  69782,  67456,  65280};

void five_bit_hd_gamma_bitshift(
    CRGB colors, CRGB colors_scale, u8 global_brightness, CRGB *out_colors,
    u8 *out_power_5bit) {

    if (global_brightness == 0) {
        *out_colors = CRGB(0, 0, 0);
        *out_power_5bit = 0;
        return;
    }

    // Step 1: Gamma Correction
    u16 r16 = gamma_2_8(colors.r);
    u16 g16 = gamma_2_8(colors.g);
    u16 b16 = gamma_2_8(colors.b);

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
void five_bit_bitshift(u16 r16, u16 g16, u16 b16,
                       u8 brightness, CRGB *out,
                       u8 *out_power_5bit) {

    // NEW in 3.10.2: A new closed form solution has been found!
    // Thank you https://github.com/gwgill!
    // It's okay if you don't know how this works, few do, but it tests
    // very well and is better than the old iterative approach which had
    // bad quantization issues (sudden jumps in brightness in certain intervals).

    auto max3 = [](u16 a, u16 b, u16 c) { return max(max(a, b), c); };

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

    u8 r8 = 0, g8 = 0, b8 = 0;

    // Apply any brightness setting (we assume brightness is 0..255)
    if (brightness != 0xff) {
        r16 = scale16by8(r16, brightness);
        g16 = scale16by8(g16, brightness);
        b16 = scale16by8(b16, brightness);
    }

    // Locate the largest value to set the brightness/scale factor
    u16 scale = max3(r16, g16, b16);

    if (scale == 0) {
        *out = CRGB(0, 0, 0);
        *out_power_5bit = 0;
        return;
    } else {
        u32 scalef;

        // Compute 5 bit quantized scale that is at or above the maximum value.
        scale = (scale + (2047 - (scale >> 5))) >> 11;

        // Adjust the 16 bit values to account for the scale, then round to 8
        // bits
        scalef = FL_PGM_READ_DWORD_NEAR(&BRIGHT_SCALE[scale]);
        r8 = (r16 * scalef + 0x808000) >> 24;
        g8 = (g16 * scalef + 0x808000) >> 24;
        b8 = (b16 * scalef + 0x808000) >> 24;

        *out = CRGB(r8, g8, b8);
        *out_power_5bit = static_cast<u8>(scale);
        return;
    }
}

} // namespace fl
