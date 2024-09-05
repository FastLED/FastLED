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
void rgb_2_rgbw_exact(uint8_t r, uint8_t g, uint8_t b, uint16_t color_temperature,
                      uint8_t* out_r, uint8_t* out_g, uint8_t* out_b, uint8_t* out_w) {
    uint8_t min_component = min3(r, g, b);
    *out_r = r - min_component;
    *out_g = g - min_component;
    *out_b = b - min_component;
    *out_w = min_component;
}

void rgb_2_rgbw_max_brightness(uint8_t r, uint8_t g, uint8_t b, uint16_t color_temperature,
                               uint8_t* out_r, uint8_t* out_g, uint8_t* out_b, uint8_t* out_w) {
    *out_r = r;
    *out_g = g;
    *out_b = b;
    *out_w = min3(r, g, b);
}

void rgb_2_rgbw(
    RGBW_MODE mode,
    uint16_t white_color_temp,
    uint8_t r, uint8_t g, uint8_t b,
    uint8_t* out_r, uint8_t* out_g, uint8_t* out_b, uint8_t* out_w
) {
    switch (mode) {
        case kInvalid:
        case kNullWhitePixel:
            *out_r = r;
            *out_g = g;
            *out_b = b;
            *out_w = 0;
            return;
        case kExactColors:
            rgb_2_rgbw_exact(r, g, b, white_color_temp, out_r, out_g, out_b, out_w);
            return;
        case kMaxBrightness:
            *out_r = r;
            *out_g = g;
            *out_b = b;
            *out_w = min3(r, g, b);
    }
    // how did we make it here?
    rgb_2_rgbw_exact(r, g, b, white_color_temp, out_r, out_g, out_b, out_w);
    static bool s_warned = false;
    if (!s_warned) {
        s_warned = true;
        printf("rgb_2_rgbw: Invalid RGBW mode %d\n", mode);
    }
}

FASTLED_NAMESPACE_END
