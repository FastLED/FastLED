/// @file    NoisePlusPalette.hpp
/// @brief   Demonstrates how to mix noise generation with color palettes on a
/// 2D LED matrix
/// @example NoisePlusPalette.hpp

#pragma once

#include <stdint.h>
#include "xymap.h"
#include "crgb.h"
#include "namespace.h"

FASTLED_NAMESPACE_BEGIN

/// @brief Performs bilinear interpolation mapping from a 2D grid to another 2D grid.
/// @param output The output grid to write into the interpolated values.
/// @param input The input grid to read from.
/// @param inputWidth The width of the input grid.
/// @param inputHeight The height of the input grid.
/// @param outputWidth The width of the output grid.
/// @param outputHeight The height of the output grid.
/// @param xyMap The XYMap to use to determine where to write the pixel. If the pixel is mapped outside of the range then it is clipped.
void bilinearExpand(CRGB *output, const CRGB *input, uint16_t inputWidth,
                    uint16_t inputHeight, uint16_t outputWidth,
                    uint16_t outputHeight, const XYMap* xyMap = nullptr);

/// @brief Performs bilinear interpolation mapping from a 2D grid to another 2D grid. This version must have power of 2 dimensions.
///        Note giving a larger dimension then the output is okay as xyMap will be used to handle this.
/// @param output The output grid to write into the interpolated values.
/// @param input The input grid to read from.
/// @param outputWidth The width of the output grid.
/// @param outputHeight The height of the output grid.
/// @param xyMap The XYMap to use to determine where to write the pixel. If the pixel is mapped outside of the range then it is clipped.
void bilinearExpandPowerOf2(CRGB *output, const CRGB *input, uint8_t outputWidth,
                            uint8_t outputHeight, const XYMap* xyMap = nullptr);



FASTLED_NAMESPACE_END
