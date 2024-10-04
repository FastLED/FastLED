/// @file    NoisePlusPalette.hpp
/// @brief   Demonstrates how to mix noise generation with color palettes on a
/// 2D LED matrix
/// @example NoisePlusPalette.hpp

#pragma once

#include <stdint.h>
#include "xymap.h"
#include "crgb.h"
// #include <iostream>

FASTLED_NAMESPACE_BEGIN

void bilinearExpand(CRGB *output, const CRGB *input, uint16_t inputWidth,
                    uint16_t inputHeight, uint16_t outputWidth,
                    uint16_t outputHeight, XYMap xyMap);

void bilinearExpandPowerOf2(CRGB *output, const CRGB *input, uint8_t width,
                            uint8_t height, XYMap xyMap);

uint8_t bilinearInterpolate(uint8_t v00, uint8_t v10, uint8_t v01,
                            uint8_t v11, uint16_t dx, uint16_t dy);

uint8_t bilinearInterpolatePowerOf2(uint8_t v00, uint8_t v10, uint8_t v01,
                                    uint8_t v11, uint8_t dx, uint8_t dy);

FASTLED_NAMESPACE_END
