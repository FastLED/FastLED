#pragma once

/// @file fl/gfx/sample.h
/// @brief 2D grid sampling with bilinear and nearest-neighbor interpolation

#include "fl/gfx/crgb.h"

namespace fl {

class XYMap;

/// Interpolation mode for sampling a 2D grid
enum SampleMode {
    SAMPLE_NEAREST,  ///< Nearest neighbor (fast, pixelated)
    SAMPLE_BILINEAR, ///< Bilinear interpolation (smooth)
};

/// @brief Sample a pixel from a 2D CRGB grid at floating-point coordinates.
/// @param grid Pixel buffer (row-major or XYMap-indexed)
/// @param xyMap Coordinate-to-index mapping for the grid
/// @param x Floating-point x coordinate
/// @param y Floating-point y coordinate
/// @param mode Interpolation mode (default: SAMPLE_BILINEAR)
CRGB sample(const CRGB *grid, const XYMap &xyMap, float x, float y,
            SampleMode mode = SAMPLE_BILINEAR);

/// @brief Bilinear interpolation sample from a 2D CRGB grid.
CRGB sampleBilinear(const CRGB *grid, const XYMap &xyMap, float x, float y);

/// @brief Nearest-neighbor sample from a 2D CRGB grid.
CRGB sampleNearest(const CRGB *grid, const XYMap &xyMap, float x, float y);

} // namespace fl
