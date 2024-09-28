

#define FASTLED_INTERNAL 1
#include "FastLED.h"
#include "five_bit_hd_gamma.h"

#include "fastled_progmem.h"
#include "lib8tion/scale8.h"
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
    *r16 = uint16_t(color.r) * color.r;
    *g16 = uint16_t(color.g) * color.g;
    *b16 = uint16_t(color.b) * color.b;
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

void five_bit_bitshift(uint16_t r16, uint16_t g16, uint16_t b16,
                       uint8_t brightness, CRGB *out, uint8_t *out_power_5bit) {
    if (!(r16 | g16 | b16) || brightness == 0) {
        *out = CRGB(0, 0, 0);
        *out_power_5bit = 0;
        return;
    }

    // Note: One day someone smarter than me will come along and invent a closed
    // form solution for this. However, the numerical method works extremely
    // well and has been optimized to avoid division performance penalties as
    // much as possible.

    // Step 1: Initialize brightness
    static const uint8_t kStartBrightness = 0b00011111;
    uint8_t v5 = kStartBrightness;

    // Step 2: Boost brightness by swapping power with the driver brightness.
    uint32_t numerator = 1;
    uint16_t denominator = 1;  // can hold all possible denominators for v5.
    // Loop while there is room to adjust brightness
    while (v5 > 1) {
        // Calculate the next reduced value of v5
        uint8_t next_v5 = v5 >> 1;
        // Update the numerator and denominator to scale brightness
        uint32_t next_numerator = numerator * v5;
        uint32_t next_denominator = denominator * next_v5;
        // Calculate the next potential brightness value
        uint32_t next_brightness_times_numerator = brightness;
        next_brightness_times_numerator *= next_numerator;
        // Check for overflow
        if (next_brightness_times_numerator > denominator * 0xff) {
            break;
        }
        numerator = next_numerator;
        denominator = next_denominator;
        v5 = next_v5;
    }
    // If brightness was adjusted, calculate the new brightness value
    if (v5 != kStartBrightness) {
        uint32_t b32 = brightness;
        b32 *= numerator;
        brightness = static_cast<uint8_t>(b32 / denominator);
    }

    // Step 3: Boost brightness of the color channels by swapping power with the
    // driver brightness.
    {
        // Initialize numerator and denominator for scaling
        uint32_t numerator = 1;
        uint16_t denominator = 1;  // can hold all possible denomintors for v5.
        uint32_t overflow = max3(r16, g16, b16);

        // Loop while v5 is greater than 1
        while (v5 > 1) {
            uint8_t next_v5 = v5 >> 1;
            uint32_t next_numerator = numerator * v5;
            uint16_t next_denominator = denominator * next_v5;
            // Calculate potential new overflow
            uint32_t next_overflow = (overflow * next_numerator);
            // Check if overflow exceeds the uint16_t limit
            if (next_overflow > next_denominator * 0xffff) {
                break;
            }
            numerator = next_numerator;
            denominator = next_denominator;
            // Update v5 for the next iteration
            v5 = next_v5;
        }

        if (numerator != 1) {  // Signal that a new value was computed.
            r16 = static_cast<uint16_t>((r16 * numerator) / denominator);
            g16 = static_cast<uint16_t>((g16 * numerator) / denominator);
            b16 = static_cast<uint16_t>((b16 * numerator) / denominator);
        }
    }

    // Step 4: scale by final brightness factor.
    if (brightness != 0xff) {
        r16 = scale16by8(r16, brightness);
        g16 = scale16by8(g16, brightness);
        b16 = scale16by8(b16, brightness);
    }

    // Step 5: Convert back to 8-bit and output.
    *out = CRGB(uint8_t(r16 >> 8), uint8_t(g16 >> 8), uint8_t(b16 >> 8));
    *out_power_5bit = v5;
}

void __builtin_five_bit_hd_gamma_bitshift(CRGB colors, CRGB colors_scale,
                                          uint8_t global_brightness,
                                          CRGB *out_colors,
                                          uint8_t *out_power_5bit) {

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
