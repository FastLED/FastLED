#pragma once

#include <stdint.h>

#include "namespace.h"

FASTLED_NAMESPACE_BEGIN

// @brief Converts RGB to RGBW using a color transfer method
// from color channels to 3x white.
// @author Jonathanese
void rgb_2_rgbw(
    uint8_t r, uint8_t g, uint8_t b,
    // As of 8/30/2024, color_temperature is ignored
    // and should just be set to 4000 as a default.
    uint16_t color_temperature,
    // output parameters
    uint8_t* out_r, uint8_t* out_g, uint8_t* out_b, uint8_t* out_w);

FASTLED_NAMESPACE_END