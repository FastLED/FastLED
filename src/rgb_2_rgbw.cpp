
#define FASTLED_INTERNAL
#include "FastLED.h"

#include <stdint.h>
#include "rgb_2_rgbw.h"
#include "namespace.h"
#include <stdio.h>

FASTLED_NAMESPACE_BEGIN

namespace {
    uint8_t min3(uint8_t a, uint8_t b, uint8_t c) {
        if (a < b) {
            if (a < c) {
                return a;
            } else {
                return c;
            }
        } else {
            if (b < c) {
                return b;
            } else {
                return c;
            }
        }
    }
}


// @brief Converts RGB to RGBW using a color transfer method
// from color channels to 3x white.
// @author Jonathanese
void rgb_2_rgbw_exact(uint16_t w_color_temperature,
                      uint8_t r, uint8_t g, uint8_t b,
                      uint8_t r_scale, uint8_t g_scale, uint8_t b_scale,
                      uint8_t* out_r, uint8_t* out_g, uint8_t* out_b, uint8_t* out_w) {
    r = scale8(r, r_scale);
    g = scale8(g, g_scale);
    b = scale8(b, b_scale);
    uint8_t min_component = min3(r, g, b);
    *out_r = r - min_component;
    *out_g = g - min_component;
    *out_b = b - min_component;
    *out_w = min_component;
}

void rgb_2_rgbw_max_brightness(uint16_t w_color_temperature,
                               uint8_t r, uint8_t g, uint8_t b,
                               uint8_t r_scale, uint8_t g_scale, uint8_t b_scale,
                               uint8_t* out_r, uint8_t* out_g, uint8_t* out_b, uint8_t* out_w) {
    *out_r = scale8(r, r_scale);
    *out_g = scale8(g, g_scale);
    *out_b = scale8(b, b_scale);
    *out_w = min3(r, g, b);
}

void rgb_2_rgbw_null_white_pixel(uint16_t w_color_temperature,
                                 uint8_t r, uint8_t g, uint8_t b,
                                 uint8_t r_scale, uint8_t g_scale, uint8_t b_scale,
                                 uint8_t* out_r, uint8_t* out_g, uint8_t* out_b, uint8_t* out_w) {
    *out_r = scale8(r, r_scale);
    *out_g = scale8(g, g_scale);
    *out_b = scale8(b, b_scale);
    *out_w = 0;
}


FASTLED_NAMESPACE_END
