#pragma once

#include <stdint.h>

#include "namespace.h"
#include "force_inline.h"

FASTLED_NAMESPACE_BEGIN

#pragma GCC push_options
#pragma GCC optimize ("O3")

enum RGBW_MODE {
    kRGBWInvalid,
    kRGBWNullWhitePixel,
    kRGBWExactColors,
    kRGBWBoostedWhite,
    kRGBWMaxBrightness,
    kRGBWUserFunction
};

enum {
    kRGBWDefaultColorTemp = 6000,
};


typedef void (*rgb_2_rgbw_function)(
    uint16_t w_color_temperature,
    uint8_t r, uint8_t g, uint8_t b,
    uint8_t r_scale, uint8_t g_scale, uint8_t b_scale,
    uint8_t* out_r, uint8_t* out_g, uint8_t* out_b, uint8_t* out_w);

/// @brief Converts RGB to RGBW using a color transfer method
/// from saturated color channels to white. This is designed to produce
/// the most accurate white point for a given color temperature and
/// reduces power usage of the chip since a white led is much more efficient
/// than three color channels of the same power mixing together. However
/// the pixel will never achieve full brightness since the white channel is
/// 3x more efficient than the color channels mixed together, so in this mode
/// the max brightness of a given pixel is reduced.
/// @example RGB(255, 255, 255) -> RGBW(0, 0, 0, 85)
/// @example RGB(255, 0, 0) -> RGBW(255, 0, 0, 0)
void rgb_2_rgbw_exact(
    uint16_t w_color_temperature,
    uint8_t r, uint8_t g, uint8_t b,
    uint8_t r_scale, uint8_t g_scale, uint8_t b_scale,
    uint8_t* out_r, uint8_t* out_g, uint8_t* out_b, uint8_t* out_w);

/// The minimum brigthness of the RGB channels is used to set the W channel.
/// This will allow the max brightness of the led chipset to be used. However
/// the leds will appear over-desaturated in this mode.
/// @example RGB(255, 255, 255) -> RGBW(255, 255, 255, 255)
/// @example RGB(1, 0, 0) -> RGBW(1, 0, 0, 1)
void rgb_2_rgbw_max_brightness(
    uint16_t w_color_temperature,
    uint8_t r, uint8_t g, uint8_t b,
    uint8_t r_scale, uint8_t g_scale, uint8_t b_scale,
    uint8_t* out_r, uint8_t* out_g, uint8_t* out_b, uint8_t* out_w);

/// @brief Converts RGB to RGBW with the W channel set to black, always.
/// @example RGB(255, 255, 255) -> RGBW(255, 255, 255, 0)
void rgb_2_rgbw_null_white_pixel(
    uint16_t w_color_temperature,
    uint8_t r, uint8_t g, uint8_t b,
    uint8_t r_scale, uint8_t g_scale, uint8_t b_scale,
    uint8_t* out_r, uint8_t* out_g, uint8_t* out_b, uint8_t* out_w);

/// @brief Converts RGB to RGBW with a boosted white channel.
void rgb_2_rgbw_white_boosted(
    uint16_t w_color_temperature,
    uint8_t r, uint8_t g, uint8_t b,
    uint8_t r_scale, uint8_t g_scale, uint8_t b_scale,
    uint8_t* out_r, uint8_t* out_g, uint8_t* out_b, uint8_t* out_w);

void rgb_2_rgbw_user_function(
    uint16_t w_color_temperature,
    uint8_t r, uint8_t g, uint8_t b,
    uint8_t r_scale, uint8_t g_scale, uint8_t b_scale,
    uint8_t* out_r, uint8_t* out_g, uint8_t* out_b, uint8_t* out_w);


void set_rgb_2_rgbw_function(rgb_2_rgbw_function func);

/// @brief Converts RGB to RGBW using one of the functions.
FASTLED_FORCE_INLINE void rgb_2_rgbw(
/// @brief Dynamic version of the rgb_w_rgbw function with less chance for the compiler to optimize.
/// @param out_w 
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
        case kRGBWBoostedWhite:
            rgb_2_rgbw_white_boosted(
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
        case kRGBWUserFunction:
            rgb_2_rgbw_user_function(
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


// @brief Converts RGB to RGBW using one of the functions.
template <RGBW_MODE MODE>
FASTLED_FORCE_INLINE void rgb_2_rgbw(
    uint16_t w_color_temperature,
    uint8_t r, uint8_t g, uint8_t b,
    uint8_t r_scale, uint8_t g_scale, uint8_t b_scale,
    uint8_t* out_r, uint8_t* out_g, uint8_t* out_b, uint8_t* out_w) {
    // We trust that the compiler will inline all of this.
    rgb_2_rgbw(
        MODE,
        w_color_temperature,
        r, g, b,
        r_scale, g_scale, b_scale,
        out_r, out_g, out_b, out_w);
}


#pragma GCC pop_options

FASTLED_NAMESPACE_END