

#define FASTLED_INTERNAL 1
#include "five_bit_hd_gamma.h"
#include "FastLED.h"

#include "fastled_progmem.h"
#include "lib8tion/intmap.h"
#include "lib8tion/math8.h"
#include "lib8tion/scale8.h"
#include "lib8tion/brightness_bitshifter.h"
#include "namespace.h"


// Author: Zach Vorhies

FASTLED_NAMESPACE_BEGIN

namespace {
template <typename T> T mymax(T a, T b) { return (a > b) ? a : b; }

template <typename T> T max3(T a, T b, T c) { return mymax(mymax(a, b), c); }

} // namespace

#ifndef FASTLED_FIVE_BIT_HD_GAMMA_FUNCTION_2_8
// Fast a memory efficient gamma=2 function.
void five_bit_hd_gamma_function(CRGB color, uint16_t *r16, uint16_t *g16,
                                uint16_t *b16) {
    uint16_t _r16 = map8_to_16(color.r);
    uint16_t _g16 = map8_to_16(color.g);
    uint16_t _b16 = map8_to_16(color.b);
    *r16 = scale16by8(_r16, color.r);
    *g16 = scale16by8(_g16, color.g);
    *b16 = scale16by8(_b16, color.b);
}
#else
// Using look up table for gamma16 correction at power of 2.8
static const uint16_t PROGMEM _gamma_2_8[256] = {
    0,     0,     0,     1,     1,     2,     4,     6,     8,     11,    14,
    18,    23,    29,    35,    41,    49,    57,    67,    77,    88,    99,
    112,   126,   141,   156,   173,   191,   210,   230,   251,   274,   297,
    322,   348,   375,   404,   433,   464,   497,   531,   566,   602,   640,
    680,   721,   763,   807,   853,   899,   948,   998,   1050,  1103,  1158,
    1215,  1273,  1333,  1394,  1458,  1523,  1590,  1658,  1729,  1801,  1875,
    1951,  2029,  2109,  2190,  2274,  2359,  2446,  2536,  2627,  2720,  2816,
    2913,  3012,  3114,  3217,  3323,  3431,  3541,  3653,  3767,  3883,  4001,
    4122,  4245,  4370,  4498,  4627,  4759,  4893,  5030,  5169,  5310,  5453,
    5599,  5747,  5898,  6051,  6206,  6364,  6525,  6688,  6853,  7021,  7191,
    7364,  7539,  7717,  7897,  8080,  8266,  8454,  8645,  8838,  9034,  9233,
    9434,  9638,  9845,  10055, 10267, 10482, 10699, 10920, 11143, 11369, 11598,
    11829, 12064, 12301, 12541, 12784, 13030, 13279, 13530, 13785, 14042, 14303,
    14566, 14832, 15102, 15374, 15649, 15928, 16209, 16493, 16781, 17071, 17365,
    17661, 17961, 18264, 18570, 18879, 19191, 19507, 19825, 20147, 20472, 20800,
    21131, 21466, 21804, 22145, 22489, 22837, 23188, 23542, 23899, 24260, 24625,
    24992, 25363, 25737, 26115, 26496, 26880, 27268, 27659, 28054, 28452, 28854,
    29259, 29667, 30079, 30495, 30914, 31337, 31763, 32192, 32626, 33062, 33503,
    33947, 34394, 34846, 35300, 35759, 36221, 36687, 37156, 37629, 38106, 38586,
    39071, 39558, 40050, 40545, 41045, 41547, 42054, 42565, 43079, 43597, 44119,
    44644, 45174, 45707, 46245, 46786, 47331, 47880, 48432, 48989, 49550, 50114,
    50683, 51255, 51832, 52412, 52996, 53585, 54177, 54773, 55374, 55978, 56587,
    57199, 57816, 58436, 59061, 59690, 60323, 60960, 61601, 62246, 62896, 63549,
    64207, 64869, 65535};

void five_bit_hd_gamma_function(CRGB rgb, uint16_t *r16, uint16_t *g16,
                                uint16_t *b16) {
    *r16 = _gamma_2_8[rgb.r];
    *g16 = _gamma_2_8[rgb.g];
    *b16 = _gamma_2_8[rgb.b];
}
#endif // FASTLED_FIVE_BIT_HD_GAMMA_FUNCTION_2_8

uint8_t five_bit_bitshift(uint16_t r16, uint16_t g16, uint16_t b16,
                          uint8_t brightness, CRGB *out, uint8_t *out_power_5bit) {
    if (!(r16 | g16 | b16) || brightness == 0) {
        *out = CRGB(map16_to_8(r16), map16_to_8(g16), map16_to_8(b16));
        *out_power_5bit = 31;
        return brightness;
    }

    // Note: One day someone smarter than me will come along and invent a closed
    // form solution for this. However, the numerical method works extremely
    // well and has been optimized to avoid division performance penalties as
    // much as possible.

    // Step 1: Initialize brightness
    static const uint8_t kStartBrightness = 0b00010000;
    uint8_t v5 = kStartBrightness;
    // Step 2: Boost brightness by swapping power with the driver brightness.
    brightness_bitshifter8(&v5, &brightness, 4);

    // Step 3: Boost brightness of the color channels by swapping power with the
    // driver brightness.
    uint16_t max_component = max3(r16, g16, b16);
    // five_bit_color_bitshift(&r16, &g16, &b16, &v5);
    uint8_t shifts = brightness_bitshifter16(&v5, &max_component, 4, 2);
    if (shifts) {
        r16 = r16 << shifts;
        g16 = g16 << shifts;
        b16 = b16 << shifts;
    }

    // Step 4: scale by final brightness factor.
    if (brightness != 0xff) {
        r16 = scale16by8(r16, brightness);
        g16 = scale16by8(g16, brightness);
        b16 = scale16by8(b16, brightness);
    }


    // brighten hardware brightness by turning on low order bits
    if (v5 > 1) {
        // since v5 is a power of two, subtracting one will invert the leading bit
        // and invert all the bits below it.
        // Example: 0b00010000 -1 = 0b00001111
        // So 0b00010000 | 0b00001111 = 0b00011111
        v5 = v5 | (v5 - 1);
    }
    // Step 5: Convert back to 8-bit and output.
    *out = CRGB(map16_to_8(r16), map16_to_8(g16), map16_to_8(b16));
    *out_power_5bit = v5;
    return brightness;
}

void __builtin_five_bit_hd_gamma_bitshift(CRGB colors, CRGB colors_scale,
                                          uint8_t global_brightness,
                                          CRGB *out_colors,
                                          uint8_t *out_power_5bit) {

    if (global_brightness == 0) {
        *out_colors = CRGB(0, 0, 0);
        *out_power_5bit = 0;
        return;
    }

    // Step 1: Gamma Correction
    uint16_t r16, g16, b16;
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

FASTLED_NAMESPACE_END
