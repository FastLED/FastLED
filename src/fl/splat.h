#pragma once

/*
"Splat" as in "splat pixel rendering" takes a pixel value in float x,y
coordinates and "splats" it into a 2x2 tile of pixel values.

Each of the four pixels in the tile is a fl::u8 value in the range
[0..255] that represents the intensity of the pixel at that point.
*/

#include "fl/tile2x2.h"
#include "fl/int.h"
#include "fl/geometry.h"

namespace fl {

/// "Splat" as in "splat pixel rendering" takes a pixel value in float x,y
/// coordinates and "splats" it into a 2x2 tile of pixel values.
///
/// Each of the four pixels in the tile is a fl::u8 value in the range
/// [0..255] that represents the intensity of the pixel at that point.
/// Tile2x2_u8 splat(vec2f xy);

Tile2x2_u8 splat(vec2f xy);

} // namespace fl
