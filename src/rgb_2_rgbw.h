#pragma once

#include <stdint.h>

#include "namespace.h"

FASTLED_NAMESPACE_BEGIN

enum RGBW_MODE {
    kRGBWInvalid,
    kRGBWNullWhitePixel,
    kRGBWExactColors,
    kRGBWMaxBrightness
};

enum {
    kRGBWDefaultColorTemp = 6500,
};


// @brief Converts RGB to RGBW using a color transfer method
// from color channels to 3x white.
// @author Jonathanese
void rgb_2_rgbw_exact(
    uint16_t w_color_temperature,
    uint8_t r, uint8_t g, uint8_t b,
    uint8_t r_scale, uint8_t g_scale, uint8_t b_scale,
    uint8_t* out_r, uint8_t* out_g, uint8_t* out_b, uint8_t* out_w);

void rgb_2_rgbw_max_brightness(
    uint16_t w_color_temperature,
    uint8_t r, uint8_t g, uint8_t b,
    uint8_t r_scale, uint8_t g_scale, uint8_t b_scale,
    uint8_t* out_r, uint8_t* out_g, uint8_t* out_b, uint8_t* out_w);

// @brief Converts RGB to RGBW with the W channel set to black.
void rgb_2_rgbw_null_white_pixel(
    uint16_t w_color_temperature,
    uint8_t r, uint8_t g, uint8_t b,
    uint8_t r_scale, uint8_t g_scale, uint8_t b_scale,
    uint8_t* out_r, uint8_t* out_g, uint8_t* out_b, uint8_t* out_w);

// @brief Converts RGB to RGBW using one of the functions.
template <RGBW_MODE MODE>
inline void rgb_2_rgbw(
    uint16_t w_color_temperature,
    uint8_t r, uint8_t g, uint8_t b,
    uint8_t r_scale, uint8_t g_scale, uint8_t b_scale,
    uint8_t* out_r, uint8_t* out_g, uint8_t* out_b, uint8_t* out_w) {
    switch (MODE) {
        case kRGBWInvalid:
        case kRGBWNullWhitePixel:
            rgb_2_rgbw_null_white_pixel(
                w_color_temperature,
                r, g, b,
                r_scale, g_scale, b_scale,
                out_r, out_g, out_b, out_w);
            return;
        case kRGBWExactColors:
            rgb_2_rgbw_exact(
                w_color_temperature,
                r, g, b,
                r_scale, g_scale, b_scale,
                out_r, out_g, out_b, out_w);
            return;
        case kRGBWMaxBrightness:
            rgb_2_rgbw_max_brightness(
                w_color_temperature,
                r, g, b,
                r_scale, g_scale, b_scale,
                out_r, out_g, out_b, out_w);
            return;
    }
    rgb_2_rgbw_null_white_pixel(
        w_color_temperature,
        r, g, b,
        r_scale, g_scale, b_scale,
        out_r, out_g, out_b, out_w);
}

inline void rgb_2_rgbw(
    RGBW_MODE mode,
    uint16_t w_color_temperature,
    uint8_t r, uint8_t g, uint8_t b,
    uint8_t r_scale, uint8_t g_scale, uint8_t b_scale,
    uint8_t* out_r, uint8_t* out_g, uint8_t* out_b, uint8_t* out_w) {
    switch (mode) {
        case kRGBWInvalid:
        case kRGBWNullWhitePixel:
            rgb_2_rgbw_null_white_pixel(
                w_color_temperature,
                r, g, b,
                r_scale, g_scale, b_scale,
                out_r, out_g, out_b, out_w);
            return;
        case kRGBWExactColors:
            rgb_2_rgbw_exact(
                w_color_temperature,
                r, g, b,
                r_scale, g_scale, b_scale,
                out_r, out_g, out_b, out_w);
            return;
        case kRGBWMaxBrightness:
            rgb_2_rgbw_max_brightness(
                w_color_temperature,
                r, g, b,
                r_scale, g_scale, b_scale,
                out_r, out_g, out_b, out_w);
            return;
    }
    rgb_2_rgbw_null_white_pixel(
        w_color_temperature,
        r, g, b,
        r_scale, g_scale, b_scale,
        out_r, out_g, out_b, out_w);
}

FASTLED_NAMESPACE_END