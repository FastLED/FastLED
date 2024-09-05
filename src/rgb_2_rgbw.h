#pragma once

#include <stdint.h>

#include "namespace.h"

FASTLED_NAMESPACE_BEGIN

enum RGBW_MODE {
    kInvalid,
    kNullWhitePixel,
    kExactColors,
    kMaxBrightness
};

enum {
    kRGBWDefaultColorTemp = 6500,
};

void rgb_2_rgbw(
    RGBW_MODE mode,
    uint16_t white_color_temp,
    uint8_t r, uint8_t g, uint8_t b,
    uint8_t* out_r, uint8_t* out_g, uint8_t* out_b, uint8_t* out_w
);

FASTLED_NAMESPACE_END