/// @file    bilinear_expansion.h
/// @brief   Demonstrates how to mix noise generation with color palettes on a
/// 2D LED matrix

#pragma once

#include <stdint.h>

#include "crgb.h"
#include "fl/namespace.h"
#include "fl/xymap.h"

namespace fl {

/// @brief Performs bilinear interpolation for upscaling an image.
/// @param output The output grid to write into the interpolated values.
/// @param input The input grid to read from.
/// @param inputWidth The width of the input grid.
/// @param inputHeight The height of the input grid.
/// @param xyMap The XYMap to use to determine where to write the pixel. If the
/// pixel is mapped outside of the range then it is clipped.
void upscaleArbitrary(const CRGB *input, CRGB *output, uint16_t inputWidth,
                      uint16_t inputHeight, fl::XYMap xyMap);

/// @brief Performs bilinear interpolation for upscaling an image.
/// @param output The output grid to write into the interpolated values.
/// @param input The input grid to read from.
/// @param inputWidth The width of the input grid.
/// @param inputHeight The height of the input grid.
/// @param xyMap The XYMap to use to determine where to write the pixel. If the
/// pixel is mapped outside of the range then it is clipped.
void upscalePowerOf2(const CRGB *input, CRGB *output, uint8_t inputWidth,
                     uint8_t inputHeight, fl::XYMap xyMap);

//
inline void upscale(const CRGB *input, CRGB *output, uint16_t inputWidth,
                    uint16_t inputHeight, fl::XYMap xyMap) {
    uint16_t outputWidth = xyMap.getWidth();
    uint16_t outputHeight = xyMap.getHeight();
    const bool wontFit =
        (outputWidth != xyMap.getWidth() || outputHeight != xyMap.getHeight());
    // if the input dimensions are not a power of 2 then we can't use the
    // optimized version.
    if (wontFit || (inputWidth & (inputWidth - 1)) ||
        (inputHeight & (inputHeight - 1))) {
        upscaleArbitrary(input, output, inputWidth, inputHeight, xyMap);
    } else {
        upscalePowerOf2(input, output, inputWidth, inputHeight, xyMap);
    }
}

// These are here for testing purposes and are slow. Their primary use
// is to test against the fixed integer version above.
void upscaleFloat(const CRGB *input, CRGB *output, uint8_t inputWidth,
                  uint8_t inputHeight, fl::XYMap xyMap);

void upscaleArbitraryFloat(const CRGB *input, CRGB *output, uint16_t inputWidth,
                           uint16_t inputHeight, fl::XYMap xyMap);

uint8_t upscaleFloat(uint8_t v00, uint8_t v10, uint8_t v01,
                                 uint8_t v11, float dx, float dy);

} // namespace fl
