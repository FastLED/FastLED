#include "FastLED.h"
#include "five_bit_hd_gamma.h"

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

    uint16_t nominator = 1;
    uint16_t denominator = 1;
    const uint16_t r16_const = r16;
    const uint16_t g16_const = g16;
    const uint16_t b16_const = b16;

    // Step 3: Bit Shifting Loop, can probably replaced with a
    // single pass bit-twiddling hack.
    do {
        {
          uint32_t next_r16 = r16 * 31 / 15;
          uint32_t next_g16 = g16 * 31 / 15;
          uint32_t next_b16 = b16 * 31 / 15;
          if (next_r16 > 0xffff) {
            break;
          }
          if (next_g16 > 0xffff) {
            break;
          }
          if (next_b16 > 0xffff) {
            break;
          }
          nominator = nominator * 31;
          denominator = denominator * 15;
          v8 = v8 >> 1;
          r16 = next_r16;
          g16 = next_g16;
          b16 = next_b16;
        }
        {
          uint32_t next_r16 = r16 * 15 / 7;
          uint32_t next_g16 = g16 * 15 / 7;
          uint32_t next_b16 = b16 * 15 / 7;
          if (next_r16 > 0xffff) {
            break;
          }
          if (next_g16 > 0xffff) {
            break;
          }
          if (next_b16 > 0xffff) {
            break;
          }
          nominator = nominator * 15;
          denominator = denominator * 7;
          v8 = v8 >> 1;
          r16 = next_r16;
          g16 = next_g16;
          b16 = next_b16;
        }
        {
          uint32_t next_r16 = r16 * 7 / 3;
          uint32_t next_g16 = g16 * 7 / 3;
          uint32_t next_b16 = b16 * 7 / 3;
          if (next_r16 > 0xffff) {
            break;
          }
          if (next_g16 > 0xffff) {
            break;
          }
          if (next_b16 > 0xffff) {
            break;
          }
          nominator = nominator * 7;
          denominator = denominator * 3;
          v8 = v8 >> 1;
          r16 = next_r16;
          g16 = next_g16;
          b16 = next_b16;
        }
        {
          uint32_t next_r16 = r16 * 3;
          uint32_t next_g16 = g16 * 3;
          uint32_t next_b16 = b16 * 3;
          if (next_r16 > 0xffff) {
            break;
          }
          if (next_g16 > 0xffff) {
            break;
          }
          if (next_b16 > 0xffff) {
            break;
          }
          nominator = nominator * 3;
          v8 = v8 >> 1;
          r16 = next_r16;
          g16 = next_g16;
          b16 = next_b16;
        }
    } while(false);

    r16 = r16_const * nominator / denominator;
    g16 = g16_const * nominator / denominator;
    b16 = b16_const * nominator / denominator;
    // protect against overflow
    if (r16 > 0xffff) {
      r16 = 0xffff;
    }
    if (g16 > 0xffff) {
      g16 = 0xffff;
    }
    if (b16 > 0xffff) {
      b16 = 0xffff;
    }

    // Step 4: Conversion Back to 8-bit.
    uint8_t r8_final = (r8 == 255 && uint8_t(r16 >> 8) >= 254) ? 255 : uint8_t(r16 >> 8);
    uint8_t g8_final = (g8 == 255 && uint8_t(g16 >> 8) >= 254) ? 255 : uint8_t(g16 >> 8);
    uint8_t b8_final = (b8 == 255 && uint8_t(b16 >> 8) >= 254) ? 255 : uint8_t(b16 >> 8);

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

    // Step 5: Output
    *out_r8 = r8_final;
    *out_g8 = g8_final;
    *out_b8 = b8_final;
    *out_power_5bit = v8;
}

FASTLED_NAMESPACE_END
