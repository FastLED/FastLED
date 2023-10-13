#include "FastLED.h"
#include "five_bit_hd_gamma.h"

#ifndef FASTLED_FIVE_BIT_HD_GAMMA_LOW_END_LINEAR_RAMP
#define FASTLED_FIVE_BIT_HD_GAMMA_LOW_END_LINEAR_RAMP 1
#endif

FASTLED_NAMESPACE_BEGIN


__attribute__((weak)) 
void five_bit_hd_gamma_function(
  uint8_t r8, uint8_t g8, uint8_t b8,
  uint16_t* r16, uint16_t* g16, uint16_t* b16) {
    *r16 = uint16_t(r8) * r8;
    *g16 = uint16_t(g8) * g8;
    *b16 = uint16_t(b8) * b8;
}

__attribute__((weak))
void five_bit_hd_gamma_bitshift(
    uint8_t r8, uint8_t g8, uint8_t b8,
    uint8_t* out_r8,
    uint8_t* out_g8,
    uint8_t* out_b8,
    uint8_t* out_power_5bit) {

    // Step 1: Gamma Correction
    uint16_t r16, g16, b16;
    five_bit_hd_gamma_function(r8, g8, b8, &r16, &g16, &b16);

    // Step 2: Initialize 5-bit brightness.
    // Note: we only get 5 levels of brightness
    uint8_t v8 = 31;

    uint16_t numerator = 1;
    uint16_t denominator = 1;
    const uint32_t r16_const = r16;
    const uint32_t g16_const = g16;
    const uint32_t b16_const = b16;

    // Step 3: Bit Shifting Loop, can probably replaced with a
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

    // Step 4: Conversion Back to 8-bit.
    uint8_t r8_final = (r8 == 255 && uint8_t(r16 >> 8) >= 254) ? 255 : uint8_t(r16 >> 8);
    uint8_t g8_final = (g8 == 255 && uint8_t(g16 >> 8) >= 254) ? 255 : uint8_t(g16 >> 8);
    uint8_t b8_final = (b8 == 255 && uint8_t(b16 >> 8) >= 254) ? 255 : uint8_t(b16 >> 8);

#if FASTLED_FIVE_BIT_HD_GAMMA_LOW_END_LINEAR_RAMP == 1
    if (v8 == 1) {
      // Linear tuning for the lowest possible brightness. x=y until
      // the intersection point at 9.
      if (r8 < 9 && r16 > 0) {
        r8_final = r8;
      }
      if (g8 < 9 && g16 > 0) {
        g8_final = g8;
      }
      if (b8 < 9 && b16 > 0) {
        b8_final = b8;
      }
    }
#endif

    // Step 5: Output
    *out_r8 = r8_final;
    *out_g8 = g8_final;
    *out_b8 = b8_final;
    *out_power_5bit = v8;
}

FASTLED_NAMESPACE_END
