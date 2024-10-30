/// @file    NoisePlusPalette.hpp
/// @brief   Demonstrates how to mix noise generation with color palettes on a
/// 2D LED matrix
/// @example NoisePlusPalette.hpp

#pragma once

#include <stdint.h>

#include "crgb.h"
#include "namespace.h"
#include "xymap.h"


FASTLED_NAMESPACE_BEGIN

/// @brief Performs bilinear interpolation for upscaling an image.
/// @param output The output grid to write into the interpolated values.
/// @param input The input grid to read from.
/// @param inputWidth The width of the input grid.
/// @param inputHeight The height of the input grid.
/// @param outputWidth The width of the output grid.
/// @param outputHeight The height of the output grid.
/// @param xyMap The XYMap to use to determine where to write the pixel. If the
/// pixel is mapped outside of the range then it is clipped.
void bilinearExpandArbitrary(const CRGB *input, CRGB *output,
                             uint16_t inputWidth, uint16_t inputHeight,
                             XYMap xyMap);

/// @brief Performs bilinear interpolation for upscaling an image.
/// @param output The output grid to write into the interpolated values.
/// @param input The input grid to read from.
/// @param outputWidth The width of the output grid.
/// @param outputHeight The height of the output grid.
/// @param xyMap The XYMap to use to determine where to write the pixel. If the
/// pixel is mapped outside of the range then it is clipped.
void bilinearExpandPowerOf2(const CRGB *input, CRGB *output, uint8_t inputWidth, uint8_t inputHeight, XYMap xyMap);

// 
inline void bilinearExpand(const CRGB *input, CRGB *output, uint16_t inputWidth,
                           uint16_t inputHeight, XYMap xyMap) {
    uint16_t outputWidth = xyMap.getWidth();
    uint16_t outputHeight = xyMap.getHeight();
    const bool wontFit = (outputWidth != xyMap.getWidth() || outputHeight != xyMap.getHeight());
    // if the input dimensions are not a power of 2 then we can't use the
    // optimized version.
    if (wontFit || (inputWidth & (inputWidth - 1)) || (inputHeight & (inputHeight - 1))) {
        bilinearExpandArbitrary(input, output, inputWidth, inputHeight, xyMap);
    } else {
        bilinearExpandPowerOf2(input, output, inputWidth, inputHeight, xyMap);
    }
}

void bilinearExpandFloat(const CRGB *input, CRGB *output,
                                 uint8_t inputWidth, uint8_t inputHeight,
                                 XYMap xyMap);

FASTLED_NAMESPACE_END
