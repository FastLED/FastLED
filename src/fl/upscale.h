/// @file    bilinear_expansion.h
/// @brief   Demonstrates how to mix noise generation with color palettes on a
/// 2D LED matrix

#pragma once

#include "fl/stdint.h"

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
void upscaleArbitrary(const CRGB *input, CRGB *output, u16 inputWidth,
                      u16 inputHeight, const fl::XYMap& xyMap);

/// @brief Performs bilinear interpolation for upscaling an image.
/// @param output The output grid to write into the interpolated values.
/// @param input The input grid to read from.
/// @param inputWidth The width of the input grid.
/// @param inputHeight The height of the input grid.
/// @param xyMap The XYMap to use to determine where to write the pixel. If the
/// pixel is mapped outside of the range then it is clipped.
void upscalePowerOf2(const CRGB *input, CRGB *output, u8 inputWidth,
                     u8 inputHeight, const fl::XYMap& xyMap);

//
inline void upscale(const CRGB *input, CRGB *output, u16 inputWidth,
                    u16 inputHeight, const fl::XYMap& xyMap) {
    u16 outputWidth = xyMap.getWidth();
    u16 outputHeight = xyMap.getHeight();
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
void upscaleFloat(const CRGB *input, CRGB *output, u8 inputWidth,
                  u8 inputHeight, const fl::XYMap& xyMap);

void upscaleArbitraryFloat(const CRGB *input, CRGB *output, u16 inputWidth,
                           u16 inputHeight, const fl::XYMap& xyMap);

u8 upscaleFloat(u8 v00, u8 v10, u8 v01,
                                 u8 v11, float dx, float dy);

} // namespace fl
