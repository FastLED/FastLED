#pragma once

/*
"Splat" as in "splat pixel rendering" takes a pixel value in float x,y
coordinates and "splats" it into a 2x2 tile of pixel values.
*/

#include "fl/tile2x2.h"
#include "fl/geometry.h"

namespace fl {

Tile2x2_u8 splat(vec2f xy);

} // namespace fl
