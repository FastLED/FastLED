/// @file rgbw.h
/// Functions for red, green, blue, white (RGBW) output

#pragma once

#include "fl/rgbw.h"

// Since FastLED 3.10.3  the RGBW functions are in the fl namespace.
// This file is a compatibility layer to allow the old functions to be used.
// It is not necessary to include this file if you are using FastLED 3.10.3 or later.

using fl::Rgbw;
using fl::RgbwInvalid;
using fl::RgbwDefault;
using fl::RgbwWhiteIsOff;
using fl::RGBW_MODE;
using fl::kRGBWDefaultColorTemp;
using fl::kRGBWInvalid;
using fl::kRGBWNullWhitePixel;
using fl::kRGBWExactColors;
using fl::kRGBWBoostedWhite;
using fl::kRGBWMaxBrightness;
using fl::kRGBWUserFunction;
