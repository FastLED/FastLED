#pragma once

#include <stdint.h>

#include "force_inline.h"
#include "namespace.h"
#include "eorder.h"

FASTLED_NAMESPACE_BEGIN


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

struct Rgbw {
    explicit Rgbw(uint16_t white_color_temp = kRGBWDefaultColorTemp,
                  RGBW_MODE rgbw_mode = kRGBWExactColors,
                  EOrderW _w_placement = WDefault)
        : white_color_temp(white_color_temp), w_placement(_w_placement),
          rgbw_mode(rgbw_mode) {}
    uint16_t white_color_temp = kRGBWDefaultColorTemp;
    EOrderW w_placement = WDefault;
    RGBW_MODE rgbw_mode = kRGBWExactColors;
    FASTLED_FORCE_INLINE bool active() const {
        return rgbw_mode != kRGBWInvalid;
    }
};

struct RgbwInvalid : public Rgbw {
    RgbwInvalid() {
        white_color_temp = kRGBWDefaultColorTemp;
        rgbw_mode = kRGBWInvalid;
    }
    static Rgbw value() {
        RgbwInvalid invalid;
        return invalid;
    }
};

struct RgbwDefault : public Rgbw {
    RgbwDefault() {
        white_color_temp = kRGBWDefaultColorTemp;
        rgbw_mode = kRGBWExactColors;
    }
    static Rgbw value() {
        RgbwDefault _default;
        return _default;
    }
};

typedef void (*rgb_2_rgbw_function)(uint16_t w_color_temperature, uint8_t r,
                                    uint8_t g, uint8_t b, uint8_t r_scale,
                                    uint8_t g_scale, uint8_t b_scale,
                                    uint8_t *out_r, uint8_t *out_g,
                                    uint8_t *out_b, uint8_t *out_w);

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
void rgb_2_rgbw_exact(uint16_t w_color_temperature, uint8_t r, uint8_t g,
                      uint8_t b, uint8_t r_scale, uint8_t g_scale,
                      uint8_t b_scale, uint8_t *out_r, uint8_t *out_g,
                      uint8_t *out_b, uint8_t *out_w);

/// The minimum brigthness of the RGB channels is used to set the W channel.
/// This will allow the max brightness of the led chipset to be used. However
/// the leds will appear over-desaturated in this mode.
/// @example RGB(255, 255, 255) -> RGBW(255, 255, 255, 255)
/// @example RGB(1, 0, 0) -> RGBW(1, 0, 0, 1)
void rgb_2_rgbw_max_brightness(uint16_t w_color_temperature, uint8_t r,
                               uint8_t g, uint8_t b, uint8_t r_scale,
                               uint8_t g_scale, uint8_t b_scale, uint8_t *out_r,
                               uint8_t *out_g, uint8_t *out_b, uint8_t *out_w);

/// @brief Converts RGB to RGBW with the W channel set to black, always.
/// @example RGB(255, 255, 255) -> RGBW(255, 255, 255, 0)
void rgb_2_rgbw_null_white_pixel(uint16_t w_color_temperature, uint8_t r,
                                 uint8_t g, uint8_t b, uint8_t r_scale,
                                 uint8_t g_scale, uint8_t b_scale,
                                 uint8_t *out_r, uint8_t *out_g, uint8_t *out_b,
                                 uint8_t *out_w);

/// @brief Converts RGB to RGBW with a boosted white channel.
void rgb_2_rgbw_white_boosted(uint16_t w_color_temperature, uint8_t r,
                              uint8_t g, uint8_t b, uint8_t r_scale,
                              uint8_t g_scale, uint8_t b_scale, uint8_t *out_r,
                              uint8_t *out_g, uint8_t *out_b, uint8_t *out_w);

void rgb_2_rgbw_user_function(uint16_t w_color_temperature, uint8_t r,
                              uint8_t g, uint8_t b, uint8_t r_scale,
                              uint8_t g_scale, uint8_t b_scale, uint8_t *out_r,
                              uint8_t *out_g, uint8_t *out_b, uint8_t *out_w);

void set_rgb_2_rgbw_function(rgb_2_rgbw_function func);

/// @brief Converts RGB to RGBW using one of the functions.
FASTLED_FORCE_INLINE void rgb_2_rgbw(
    /// @brief Dynamic version of the rgb_w_rgbw function with less chance for
    /// the compiler to optimize.
    /// @param out_w
    RGBW_MODE mode, uint16_t w_color_temperature, uint8_t r, uint8_t g,
    uint8_t b, uint8_t r_scale, uint8_t g_scale, uint8_t b_scale,
    uint8_t *out_r, uint8_t *out_g, uint8_t *out_b, uint8_t *out_w) {
    switch (mode) {
    case kRGBWInvalid:
    case kRGBWNullWhitePixel:
        rgb_2_rgbw_null_white_pixel(w_color_temperature, r, g, b, r_scale,
                                    g_scale, b_scale, out_r, out_g, out_b,
                                    out_w);
        return;
    case kRGBWExactColors:
        rgb_2_rgbw_exact(w_color_temperature, r, g, b, r_scale, g_scale,
                         b_scale, out_r, out_g, out_b, out_w);
        return;
    case kRGBWBoostedWhite:
        rgb_2_rgbw_white_boosted(w_color_temperature, r, g, b, r_scale, g_scale,
                                 b_scale, out_r, out_g, out_b, out_w);
        return;
    case kRGBWMaxBrightness:
        rgb_2_rgbw_max_brightness(w_color_temperature, r, g, b, r_scale,
                                  g_scale, b_scale, out_r, out_g, out_b, out_w);
        return;
    case kRGBWUserFunction:
        rgb_2_rgbw_user_function(w_color_temperature, r, g, b, r_scale, g_scale,
                                 b_scale, out_r, out_g, out_b, out_w);
        return;
    }
    rgb_2_rgbw_null_white_pixel(w_color_temperature, r, g, b, r_scale, g_scale,
                                b_scale, out_r, out_g, out_b, out_w);
}

// @brief Converts RGB to RGBW using one of the functions.
template <RGBW_MODE MODE>
FASTLED_FORCE_INLINE void
rgb_2_rgbw(uint16_t w_color_temperature, uint8_t r, uint8_t g, uint8_t b,
           uint8_t r_scale, uint8_t g_scale, uint8_t b_scale, uint8_t *out_r,
           uint8_t *out_g, uint8_t *out_b, uint8_t *out_w) {
    // We trust that the compiler will inline all of this.
    rgb_2_rgbw(MODE, w_color_temperature, r, g, b, r_scale, g_scale, b_scale,
               out_r, out_g, out_b, out_w);
}

// Assuming all RGB pixels are already ordered in native led ordering, then this
// function will reorder them so that white is also the correct position.
// b0-b2 are actually rgb that are already in native LED order.
// and out_b0-out_b3 are the output RGBW in native LED chipset order.
// w is the white component that needs to be inserted into the RGB data at
// the correct position.
void rgbw_partial_reorder(EOrderW w_placement, uint8_t b0, uint8_t b1,
                          uint8_t b2, uint8_t w, uint8_t *out_b0,
                          uint8_t *out_b1, uint8_t *out_b2, uint8_t *out_b3);


FASTLED_NAMESPACE_END