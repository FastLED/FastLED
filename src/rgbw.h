/// @file rgbw.h
/// Functions for red, green, blue, white (RGBW) output

#pragma once

#include "fl/rgbw.h"

using Rgbw = fl::Rgbw;
using RgbwInvalid = fl::RgbwInvalid;
using RgbwDefault = fl::RgbwDefault;
using RgbwWhiteIsOff = fl::RgbwWhiteIsOff;
using RGBW_MODE = fl::RGBW_MODE;

enum {
    kRGBWDefaultColorTemp = fl::kRGBWDefaultColorTemp,
};

constexpr RGBW_MODE kRGBWInvalid = fl::kRGBWInvalid;
constexpr RGBW_MODE kRGBWNullWhitePixel = fl::kRGBWNullWhitePixel;
constexpr RGBW_MODE kRGBWExactColors = fl::kRGBWExactColors;
constexpr RGBW_MODE kRGBWBoostedWhite = fl::kRGBWBoostedWhite;
constexpr RGBW_MODE kRGBWMaxBrightness = fl::kRGBWMaxBrightness;
constexpr RGBW_MODE kRGBWUserFunction = fl::kRGBWUserFunction;


template <fl::RGBW_MODE MODE>
void rgb_2_rgbw(uint16_t w_color_temperature, uint8_t r, uint8_t g, uint8_t b,
                uint8_t r_scale, uint8_t g_scale, uint8_t b_scale,
                uint8_t *out_r, uint8_t *out_g, uint8_t *out_b,
                uint8_t *out_w) {
    fl::rgb_2_rgbw(MODE, w_color_temperature, r, g, b, r_scale, g_scale,
                   b_scale, out_r, out_g, out_b, out_w);
}

inline void rgbw_partial_reorder(fl::EOrderW w_placement, uint8_t b0,
                                 uint8_t b1, uint8_t b2, uint8_t w,
                                 uint8_t *out_b0, uint8_t *out_b1,
                                 uint8_t *out_b2, uint8_t *out_b3) {
    fl::rgbw_partial_reorder(w_placement, b0, b1, b2, w, out_b0, out_b1, out_b2,
                             out_b3);
}
