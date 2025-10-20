/// @file chsv.h
/// Defines the hue, saturation, and value (HSV) pixel struct

#pragma once

/*
Legacy header. Prefer to use fl/hsv.h instead.
*/
#include "fl/hsv.h"
// Backward compatibility: bring fl::hsv8 into global namespace as CHSV
using CHSV = fl::hsv8;
using fl::HSVHue;
using fl::HUE_RED;
using fl::HUE_ORANGE;
using fl::HUE_YELLOW;
using fl::HUE_GREEN;
using fl::HUE_AQUA;
using fl::HUE_BLUE;
using fl::HUE_PURPLE;
using fl::HUE_PINK;
