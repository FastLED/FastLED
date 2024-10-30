
#include <stdint.h>

#define FASTLED_INTERNAL
#include "FastLED.h"

#include "rgbw.h"


FASTLED_NAMESPACE_BEGIN

namespace {
inline uint8_t min3(uint8_t a, uint8_t b, uint8_t c) {
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

inline uint8_t divide_by_3(uint8_t x) {
    uint16_t y = (uint16_t(x) * 85) >> 8;
    return static_cast<uint8_t>(y);
}
} // namespace

// @brief Converts RGB to RGBW using a color transfer method
// from color channels to 3x white.
// @author Jonathanese
void rgb_2_rgbw_exact(uint16_t w_color_temperature, uint8_t r, uint8_t g,
                      uint8_t b, uint8_t r_scale, uint8_t g_scale,
                      uint8_t b_scale, uint8_t *out_r, uint8_t *out_g,
                      uint8_t *out_b, uint8_t *out_w) {
    r = scale8(r, r_scale);
    g = scale8(g, g_scale);
    b = scale8(b, b_scale);
    uint8_t min_component = min3(r, g, b);
    *out_r = r - min_component;
    *out_g = g - min_component;
    *out_b = b - min_component;
    *out_w = min_component;
}

void rgb_2_rgbw_max_brightness(uint16_t w_color_temperature, uint8_t r,
                               uint8_t g, uint8_t b, uint8_t r_scale,
                               uint8_t g_scale, uint8_t b_scale, uint8_t *out_r,
                               uint8_t *out_g, uint8_t *out_b, uint8_t *out_w) {
    *out_r = scale8(r, r_scale);
    *out_g = scale8(g, g_scale);
    *out_b = scale8(b, b_scale);
    *out_w = min3(r, g, b);
}

void rgb_2_rgbw_null_white_pixel(uint16_t w_color_temperature, uint8_t r,
                                 uint8_t g, uint8_t b, uint8_t r_scale,
                                 uint8_t g_scale, uint8_t b_scale,
                                 uint8_t *out_r, uint8_t *out_g, uint8_t *out_b,
                                 uint8_t *out_w) {
    *out_r = scale8(r, r_scale);
    *out_g = scale8(g, g_scale);
    *out_b = scale8(b, b_scale);
    *out_w = 0;
}

void rgb_2_rgbw_white_boosted(uint16_t w_color_temperature, uint8_t r,
                              uint8_t g, uint8_t b, uint8_t r_scale,
                              uint8_t g_scale, uint8_t b_scale, uint8_t *out_r,
                              uint8_t *out_g, uint8_t *out_b, uint8_t *out_w) {
    r = scale8(r, r_scale);
    g = scale8(g, g_scale);
    b = scale8(b, b_scale);
    uint8_t min_component = min3(r, g, b);
    uint8_t w;
    bool is_min = true;
    if (min_component <= 84) {
        w = 3 * min_component;
    } else {
        w = 255;
        is_min = false;
    }
    uint8_t r_prime;
    uint8_t g_prime;
    uint8_t b_prime;
    if (is_min) {
        r_prime = r - min_component;
        g_prime = g - min_component;
        b_prime = b - min_component;
    } else {
        uint8_t w3 = divide_by_3(w);
        r_prime = r - w3;
        g_prime = g - w3;
        b_prime = b - w3;
    }

    *out_r = r_prime;
    *out_g = g_prime;
    *out_b = b_prime;
    *out_w = w;
}

rgb_2_rgbw_function g_user_function = rgb_2_rgbw_exact;

void set_rgb_2_rgbw_function(rgb_2_rgbw_function func) {
    if (func == nullptr) {
        g_user_function = rgb_2_rgbw_exact;
        return;
    }
    g_user_function = func;
}

void rgb_2_rgbw_user_function(uint16_t w_color_temperature, uint8_t r,
                              uint8_t g, uint8_t b, uint8_t r_scale,
                              uint8_t g_scale, uint8_t b_scale, uint8_t *out_r,
                              uint8_t *out_g, uint8_t *out_b, uint8_t *out_w) {
    g_user_function(w_color_temperature, r, g, b, r_scale, g_scale, b_scale,
                    out_r, out_g, out_b, out_w);
}

void rgbw_partial_reorder(EOrderW w_placement, uint8_t b0, uint8_t b1,
                          uint8_t b2, uint8_t w, uint8_t *out_b0,
                          uint8_t *out_b1, uint8_t *out_b2, uint8_t *out_b3) {

    uint8_t out[4] = {b0, b1, b2, 0};
    switch (w_placement) {
    // unrolled loop for speed.
    case W3:
        out[3] = w;
        break;
    case W2:
        out[3] = out[2];  // memmove and copy.
        out[2] = w;
        break;
    case W1:
        out[3] = out[2];
        out[2] = out[1];
        out[1] = w;
        break;
    case W0:
        out[3] = out[2];
        out[2] = out[1];
        out[1] = out[0];
        out[0] = w;
        break;
    }
    *out_b0 = out[0];
    *out_b1 = out[1];
    *out_b2 = out[2];
    *out_b3 = out[3];
}

FASTLED_NAMESPACE_END
