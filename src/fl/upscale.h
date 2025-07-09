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

/// @brief Optimized upscale for rectangular/line-by-line XY maps.
/// @param input The input grid to read from.
/// @param output The output grid to write into the interpolated values.
/// @param inputWidth The width of the input grid.
/// @param inputHeight The height of the input grid.
/// @param outputWidth The width of the output grid.
/// @param outputHeight The height of the output grid.
/// This version bypasses XY mapping overhead for rectangular layouts.
void upscaleRectangular(const CRGB *input, CRGB *output, u16 inputWidth,
                        u16 inputHeight, u16 outputWidth, u16 outputHeight);

/// @brief Optimized upscale for rectangular/line-by-line XY maps (power-of-2 version).
/// @param input The input grid to read from.
/// @param output The output grid to write into the interpolated values.
/// @param inputWidth The width of the input grid (must be power of 2).
/// @param inputHeight The height of the input grid (must be power of 2).
/// @param outputWidth The width of the output grid (must be power of 2).
/// @param outputHeight The height of the output grid (must be power of 2).
/// This version bypasses XY mapping overhead for rectangular layouts.
void upscaleRectangularPowerOf2(const CRGB *input, CRGB *output, u8 inputWidth,
                                u8 inputHeight, u8 outputWidth, u8 outputHeight);

//
inline void upscale(const CRGB *input, CRGB *output, u16 inputWidth,
                    u16 inputHeight, const fl::XYMap& xyMap) {
    u16 outputWidth = xyMap.getWidth();
    u16 outputHeight = xyMap.getHeight();
    const bool wontFit =
        (outputWidth != xyMap.getWidth() || outputHeight != xyMap.getHeight());
    
    // Check if we can use the optimized rectangular version
    const bool isRectangular = (xyMap.getType() == XYMap::kLineByLine);
    
    if (isRectangular) {
        // Use optimized rectangular version that bypasses XY mapping
        if (wontFit || (inputWidth & (inputWidth - 1)) ||
            (inputHeight & (inputHeight - 1))) {
            upscaleRectangular(input, output, inputWidth, inputHeight, 
                              outputWidth, outputHeight);
        } else {
            upscaleRectangularPowerOf2(input, output, inputWidth, inputHeight,
                                      outputWidth, outputHeight);
        }
    } else {
        // Use the original XY-mapped versions
    if (wontFit || (inputWidth & (inputWidth - 1)) ||
        (inputHeight & (inputHeight - 1))) {
        upscaleArbitrary(input, output, inputWidth, inputHeight, xyMap);
    } else {
        upscalePowerOf2(input, output, inputWidth, inputHeight, xyMap);
        }
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
