// ring_screenmap.h - Circular ScreenMap that samples from a rectangular grid
#pragma once

#include "fl/math/screenmap.h"

// Build a ScreenMap that places LEDs in a circle within a rectangular grid.
fl::ScreenMap makeRingScreenMap(int numLeds, int gridWidth, int gridHeight,
                                float diameter = 0.15f);
