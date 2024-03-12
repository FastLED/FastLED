#include "FastLED.h"
#include "five_bit_hd_gamma.h"
#include "fastled_progmem.h"
#include "lib8tion/scale8.h"

// Author: Zach Vorhies

#ifndef FASTLED_FIVE_BIT_HD_GAMMA_LOW_END_LINEAR_RAMP
#define FASTLED_FIVE_BIT_HD_GAMMA_LOW_END_LINEAR_RAMP 0
#endif

FASTLED_NAMESPACE_BEGIN

#ifndef FASTLED_FIVE_BIT_HD_GAMMA_BITSHIFT_FUNCTION_OVERRIDE
#ifndef FASTLED_FIVE_BIT_HD_GAMMA_FUNCTION_2_8
// Fast a memory efficient gamma=2 function.
void five_bit_hd_gamma_function(
  uint8_t r8, uint8_t g8, uint8_t b8,
  uint16_t* r16, uint16_t* g16, uint16_t* b16) {
    *r16 = uint16_t(r8) * r8;
    *g16 = uint16_t(g8) * g8;
    *b16 = uint16_t(b8) * b8;
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

void five_bit_hd_gamma_function(uint8_t r8, uint8_t g8,
                                uint8_t b8, uint16_t *r16,
                                uint16_t *g16,
                                uint16_t *b16) {
  *r16 = _gamma_2_8[r8];
  *g16 = _gamma_2_8[g8];
  *b16 = _gamma_2_8[b8];
}
#endif  // FASTLED_FIVE_BIT_HD_GAMMA_FUNCTION_2_8
#endif  // FASTLED_FIVE_BIT_HD_GAMMA_BITSHIFT_FUNCTION_OVERRIDE

#ifndef FASTLED_FIVE_BIT_HD_BITSHIFT_FUNCTION_OVERRIDE

void five_bit_hd_gamma_bitshift(
    uint8_t r8, uint8_t g8, uint8_t b8,
    uint8_t r8_scale, uint8_t g8_scale, uint8_t b8_scale,
    uint8_t* out_r8,
    uint8_t* out_g8,
    uint8_t* out_b8,
    uint8_t* out_power_5bit) {

    // Step 1: Gamma Correction
    uint16_t r16, g16, b16;
    five_bit_hd_gamma_function(r8, g8, b8, &r16, &g16, &b16);

    // Step 2: Post gamma correction scale.
    if (r8_scale != 0xff || g8_scale != 0xff || b8_scale != 0xff) {
      r16 = scale16by8(r16, r8_scale);
      g16 = scale16by8(g16, g8_scale);
      b16 = scale16by8(b16, b8_scale);
    }

    // Step 3: Initialize 5-bit brightness.
    // Note: we only get 5 levels of brightness
    uint8_t v8 = 31;

    uint16_t numerator = 1;
    uint16_t denominator = 1;
    const uint32_t r16_const = r16;
    const uint32_t g16_const = g16;
    const uint32_t b16_const = b16;

    // Step 4: Bit Shifting Loop, can probably replaced with a
    // single pass bit-twiddling hack.
    do {
        // Note that to avoid slow divisions, we multiply the max_value
        // by the denominator.
        uint32_t max_value = 0xfffful * 15;
        if (r16_const * 31 > max_value) {
          break;
        }
        if (g16_const * 31 > max_value) {
          break;
        }
        if (b16_const * 31 > max_value) {
          break;
        }
        numerator = 31;
        denominator = 15;
        v8 = 15;

        max_value = 0xfffful * 15 * 7;
        if (r16_const * 31 * 15 > max_value) {
          break;
        }
        if (g16_const * 31 * 15 > max_value) {
          break;
        }
        if (b16_const * 31 * 15 > max_value) {
          break;
        }
        numerator = 31 * 15;
        denominator = 15 * 7;
        v8 = 7;

        max_value = 0xfffful * 15 * 7 * 3;
        if (r16_const * 31 * 15 * 7 > max_value) {
          break;
        }
        if (g16_const * 31 * 15 * 7 > max_value) {
          break;
        }
        if (b16_const * 31 * 15 * 7 > max_value) {
          break;
        }
        numerator = 31 * 15 * 7;
        denominator = 15 * 7 * 3;
        v8 = 3;

        max_value = 0xfffful * 15 * 7 * 3;
        if (r16_const * 31 * 15 * 7 * 3 > max_value) {
          break;
        }
        if (g16_const * 31 * 15 * 7 * 3 > max_value) {
          break;
        }
        if (b16_const * 31 * 15 * 7 * 3 > max_value) {
          break;
        }
        numerator = 31 * 15 * 7 * 3;
        v8 = 1;
    } while(false);

    r16 = uint16_t(r16_const * numerator / denominator);
    g16 = uint16_t(g16_const * numerator / denominator);
    b16 = uint16_t(b16_const * numerator / denominator);

    // Step 5: Conversion Back to 8-bit.
    uint8_t r8_final = (r8 == 255 && uint8_t(r16 >> 8) >= 254) ? 255 : uint8_t(r16 >> 8);
    uint8_t g8_final = (g8 == 255 && uint8_t(g16 >> 8) >= 254) ? 255 : uint8_t(g16 >> 8);
    uint8_t b8_final = (b8 == 255 && uint8_t(b16 >> 8) >= 254) ? 255 : uint8_t(b16 >> 8);

#if FASTLED_FIVE_BIT_HD_GAMMA_LOW_END_LINEAR_RAMP > 0
    if (v8 == 1) {
      // Linear tuning for the lowest possible brightness. x=y until
      // the intersection point at 9.
      if (r8 < FASTLED_FIVE_BIT_HD_GAMMA_LOW_END_LINEAR_RAMP && r16 > 0) {
        r8_final = r8;
      }
      if (g8 < FASTLED_FIVE_BIT_HD_GAMMA_LOW_END_LINEAR_RAMP && g16 > 0) {
        g8_final = g8;
      }
      if (b8 < FASTLED_FIVE_BIT_HD_GAMMA_LOW_END_LINEAR_RAMP && b16 > 0) {
        b8_final = b8;
      }
    }
#endif

    // Step 6: Output
    *out_r8 = r8_final;
    *out_g8 = g8_final;
    *out_b8 = b8_final;
    *out_power_5bit = v8;
}

#endif // FASTLED_FIVE_BIT_HD_BITSHIFT_FUNCTION_OVERRIDE

FASTLED_NAMESPACE_END
