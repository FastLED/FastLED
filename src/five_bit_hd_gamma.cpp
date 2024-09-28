

#define FASTLED_INTERNAL 1

#include "FastLED.h"
#include "five_bit_hd_gamma.h"
#include "fastled_progmem.h"
#include "lib8tion/scale8.h"
#include "namespace.h"

// Author: Zach Vorhies

FASTLED_NAMESPACE_BEGIN

namespace
{
  template<typename T>
  T mymax(T a, T b) {
    return (a > b) ? a : b;
  }

  template<typename T>
  T max3(T a, T b, T c) {
    return mymax(mymax(a, b), c);
  }
  
} // namespace

#ifndef FASTLED_FIVE_BIT_HD_GAMMA_FUNCTION_2_8
// Fast a memory efficient gamma=2 function.
void five_bit_hd_gamma_function(CRGB color,
                                uint16_t* r16, uint16_t* g16, uint16_t* b16) {
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

void five_bit_hd_gamma_function(CRGB rgb,
                                uint16_t *r16,
                                uint16_t *g16,
                                uint16_t *b16) {
  *r16 = _gamma_2_8[rgb.r];
  *g16 = _gamma_2_8[rgb.g];
  *b16 = _gamma_2_8[rgb.b];
}
#endif  // FASTLED_FIVE_BIT_HD_GAMMA_FUNCTION_2_8


void five_bit_bitshift(uint16_t r16, uint16_t g16, uint16_t b16, uint8_t brightness, CRGB* out, uint8_t* out_power_5bit) {
    // Step 3: Initialize 5-bit brightness.
    // Note: we only get 5 levels of brightness
    uint8_t v8 = 0b00011111;

    // global brightness trades bits with 5 bit power brightness to improve
    // final color resolution. The shifted global brightness is then applied
    // at the end so it has better resolution and doesn't truncate bits.
    if (brightness <= 85) {
      while (v8 > 1 && (brightness <= 85)) {
        // each bitshift down for v8 is a divide by 3, hence we multiply by 3 for the global brightness.
        brightness = (brightness << 1) | 0x1;  // global brightness shifts up
        v8 >>= 1;                                            // driver bit brightness shifts down
      }
    }
    {
      uint32_t overflow = max3(r16, g16, b16);
      while (v8 > 1) {
        overflow = (overflow << 1) | 1;
        if (overflow > 0xffff) {
          break;
        }
        v8 >>= 1;
        r16 = (r16 << 1) | 1;
        g16 = (g16 << 1) | 1;
        b16 = (b16 << 1) | 1;
      }
    }

    // now apply the rest of the global_brightness that has been bitshifted up.
    if (brightness != 0xff) {
      r16 = scale16by8(r16, brightness);
      g16 = scale16by8(g16, brightness);
      b16 = scale16by8(b16, brightness);
    }

    // Step 5: Conversion Back to 8-bit.
    uint8_t r8_final = (uint8_t(r16 >> 8) >= 254) ? 255 : uint8_t(r16 >> 8);
    uint8_t g8_final = (uint8_t(g16 >> 8) >= 254) ? 255 : uint8_t(g16 >> 8);
    uint8_t b8_final = (uint8_t(b16 >> 8) >= 254) ? 255 : uint8_t(b16 >> 8);

    // Step 6: Output
    *out = CRGB(r8_final, g8_final, b8_final);
    *out_power_5bit = v8;
}

void __builtin_five_bit_hd_gamma_bitshift(
    CRGB colors,
    CRGB colors_scale,
    uint8_t global_brightness,
    CRGB* out_colors,
    uint8_t* out_power_5bit) {

    // Step 1: Gamma Correction
    uint16_t r16, g16, b16;
    five_bit_hd_gamma_function(colors, &r16, &g16, &b16);

    // Step 2: Post gamma correction scale.
    // Note that the colors_scale is expected to be the high range and should
    // represent color correction.
    if (colors_scale.r != 0xff) {
      r16 = scale16by8(r16, colors_scale.r);
    }
    if (colors_scale.g != 0xff) {
      g16 = scale16by8(g16, colors_scale.g);
    }
    if (colors_scale.b != 0xff) {
      b16 = scale16by8(b16, colors_scale.b);
    }

    five_bit_bitshift(r16, g16, b16, global_brightness, out_colors, out_power_5bit);
}


FASTLED_NAMESPACE_END
