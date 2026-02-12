/// @file rgbw.cpp
/// Functions for red, green, blue, white (RGBW) output

#include "fl/stl/stdint.h"

#define FASTLED_INTERNAL
#include "fl/fastled.h"

#include "rgbw.h"


namespace fl {


namespace {
inline u8 min3(u8 a, u8 b, u8 c) {
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

inline u8 divide_by_3(u8 x) {
    u16 y = (u16(x) * 85) >> 8;
    return static_cast<u8>(y);
}
} // namespace

// @brief Converts RGB to RGBW using a color transfer method
// from color channels to 3x white.
// @author Jonathanese
void rgb_2_rgbw_exact(u16 w_color_temperature, u8 r, u8 g,
                      u8 b, u8 r_scale, u8 g_scale,
                      u8 b_scale, u8 *out_r, u8 *out_g,
                      u8 *out_b, u8 *out_w) {
    (void)w_color_temperature;
    r = scale8(r, r_scale);
    g = scale8(g, g_scale);
    b = scale8(b, b_scale);
    u8 min_component = min3(r, g, b);
    *out_r = r - min_component;
    *out_g = g - min_component;
    *out_b = b - min_component;
    *out_w = min_component;
}

void rgb_2_rgbw_max_brightness(u16 w_color_temperature, u8 r,
                               u8 g, u8 b, u8 r_scale,
                               u8 g_scale, u8 b_scale, u8 *out_r,
                               u8 *out_g, u8 *out_b, u8 *out_w) {
    (void)w_color_temperature;
    *out_r = scale8(r, r_scale);
    *out_g = scale8(g, g_scale);
    *out_b = scale8(b, b_scale);
    *out_w = min3(*out_r, *out_g, *out_b);
}

void rgb_2_rgbw_null_white_pixel(u16 w_color_temperature, u8 r,
                                 u8 g, u8 b, u8 r_scale,
                                 u8 g_scale, u8 b_scale,
                                 u8 *out_r, u8 *out_g, u8 *out_b,
                                 u8 *out_w) {
    (void)w_color_temperature;
    *out_r = scale8(r, r_scale);
    *out_g = scale8(g, g_scale);
    *out_b = scale8(b, b_scale);
    *out_w = 0;
}

void rgb_2_rgbw_white_boosted(u16 w_color_temperature, u8 r,
                              u8 g, u8 b, u8 r_scale,
                              u8 g_scale, u8 b_scale, u8 *out_r,
                              u8 *out_g, u8 *out_b, u8 *out_w) {
    (void)w_color_temperature;
    r = scale8(r, r_scale);
    g = scale8(g, g_scale);
    b = scale8(b, b_scale);
    u8 min_component = min3(r, g, b);
    u8 w;
    bool is_min = true;
    if (min_component <= 84) {
        w = 3 * min_component;
    } else {
        w = 255;
        is_min = false;
    }
    u8 r_prime;
    u8 g_prime;
    u8 b_prime;
    if (is_min) {
        r_prime = r - min_component;
        g_prime = g - min_component;
        b_prime = b - min_component;
    } else {
        u8 w3 = divide_by_3(w);
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

void rgb_2_rgbw_user_function(u16 w_color_temperature, u8 r,
                              u8 g, u8 b, u8 r_scale,
                              u8 g_scale, u8 b_scale, u8 *out_r,
                              u8 *out_g, u8 *out_b, u8 *out_w) {
    g_user_function(w_color_temperature, r, g, b, r_scale, g_scale, b_scale,
                    out_r, out_g, out_b, out_w);
}

void rgbw_partial_reorder(EOrderW w_placement, u8 b0, u8 b1,
                          u8 b2, u8 w, u8 *out_b0,
                          u8 *out_b1, u8 *out_b2, u8 *out_b3) {

    u8 out[4] = {b0, b1, b2, 0};
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

} // namespace fl
