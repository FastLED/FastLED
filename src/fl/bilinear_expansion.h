/// @file    bilinear_expansion.h
/// @brief   Demonstrates how to mix noise generation with color palettes on a
/// 2D LED matrix

#pragma once

#include <stdint.h>

#include "crgb.h"
#include "fl/deprecated.h"
#include "fl/namespace.h"
#include "fl/upscale.h"
#include "fl/xymap.h"

namespace fl {

/// @brief Performs bilinear interpolation for upscaling an image.
/// @param output The output grid to write into the interpolated values.
/// @param input The input grid to read from.
/// @param inputWidth The width of the input grid.
/// @param inputHeight The height of the input grid.
/// @param xyMap The XYMap to use to determine where to write the pixel. If the
/// pixel is mapped outside of the range then it is clipped.

void bilinearExpandArbitrary(const CRGB *input, CRGB *output,
                             uint16_t inputWidth, uint16_t inputHeight,
                             fl::XYMap xyMap)
    FASTLED_DEPRECATED("use upscaleArbitrary from upscale.h");

void bilinearExpandPowerOf2(const CRGB *input, CRGB *output, uint8_t inputWidth,
                            uint8_t inputHeight, fl::XYMap xyMap)
    FASTLED_DEPRECATED("use upscalePowerOf2 from upscale.h");

void bilinearExpand(const CRGB *input, CRGB *output, uint16_t inputWidth,
                    uint16_t inputHeight, fl::XYMap xyMap)
    FASTLED_DEPRECATED("use upscale from upscale.h");

void bilinearExpandFloat(const CRGB *input, CRGB *output, uint8_t inputWidth,
                         uint8_t inputHeight, fl::XYMap xyMap)
    FASTLED_DEPRECATED("use upscaleFloat from upscale.h");

void bilinearExpandArbitraryFloat(const CRGB *input, CRGB *output,
                                  uint16_t inputWidth, uint16_t inputHeight,
                                  fl::XYMap xyMap)
    FASTLED_DEPRECATED("use upscaleArbitraryFloat from upscale.h");

uint8_t bilinearInterpolateFloat(uint8_t v00, uint8_t v10, uint8_t v01,
                                 uint8_t v11, float dx, float dy)
    FASTLED_DEPRECATED("use upscaleFloat from upscale.h");

////////////////// Inline definitions for backward compatibility ///////////////////////

inline void bilinearExpandArbitrary(const CRGB *input, CRGB *output,
                                    uint16_t inputWidth, uint16_t inputHeight,
                                    fl::XYMap xyMap) {
    upscaleArbitrary(input, output, inputWidth, inputHeight, xyMap);
}

inline void bilinearExpandPowerOf2(const CRGB *input, CRGB *output,
                                   uint8_t inputWidth, uint8_t inputHeight,
                                   fl::XYMap xyMap) {
    upscalePowerOf2(input, output, inputWidth, inputHeight, xyMap);
}

inline void bilinearExpand(const CRGB *input, CRGB *output, uint16_t inputWidth,
                           uint16_t inputHeight, fl::XYMap xyMap) {
    upscale(input, output, inputWidth, inputHeight, xyMap);
}

inline void bilinearExpandArbitraryFloat(const CRGB *input, CRGB *output,
                                         uint16_t inputWidth,
                                         uint16_t inputHeight,
                                         fl::XYMap xyMap) {
    upscaleArbitraryFloat(input, output, inputWidth, inputHeight, xyMap);
}

inline uint8_t bilinearInterpolateFloat(uint8_t v00, uint8_t v10, uint8_t v01,
                                        uint8_t v11, float dx, float dy) {
    return upscaleFloat(v00, v10, v01, v11, dx, dy);
}

} // namespace fl
