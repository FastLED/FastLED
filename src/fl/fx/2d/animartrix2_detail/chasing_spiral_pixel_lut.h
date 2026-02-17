#pragma once

// Per-pixel pre-computed values for Chasing_Spirals inner loop
// Extracted from animartrix2_detail.hpp
//
// Per-pixel pre-computed s16x16 values for Chasing_Spirals inner loop.
// These are constant per-frame (depend only on grid geometry, not time).
// Chasing_Spirals-specific LUT.

#include "fl/fixed_point/s16x16.h"
#include "fl/stl/stdint.h"


namespace fl {


struct ChasingSpiralPixelLUT {
    fl::s16x16 base_angle;     // 3*theta - dist/3
    fl::s16x16 dist_scaled;    // distance * scale (0.1), pre-scaled for noise coords
    fl::s16x16 rf3;            // 3 * radial_filter (for red channel)
    fl::s16x16 rf_half;        // radial_filter / 2 (for green channel)
    fl::s16x16 rf_quarter;     // radial_filter / 4 (for blue channel)
    fl::u16 pixel_idx;        // Pre-computed xyMap(x, y) output pixel index
};


}
