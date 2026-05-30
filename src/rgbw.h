/// @file rgbw.h
/// Functions for red, green, blue, white (RGBW) output

#pragma once

#include "fl/gfx/rgbw.h"

// Since FastLED 3.10.3  the RGBW functions are in the fl namespace.
// This file is a compatibility layer to allow the old functions to be used.
// It is not necessary to include this file if you are using FastLED 3.10.3 or later.

using fl::Rgbw;
using fl::RgbwInvalid;
using fl::RgbwDefault;
using fl::RgbwWhiteIsOff;
using fl::RGBW_MODE;
using fl::kRGBWDefaultColorTemp;
// Backward-compat constexpr aliases for enum class values
constexpr RGBW_MODE kRGBWInvalid = RGBW_MODE::kRGBWInvalid;
constexpr RGBW_MODE kRGBWNullWhitePixel = RGBW_MODE::kRGBWNullWhitePixel;
constexpr RGBW_MODE kRGBWExactColors = RGBW_MODE::kRGBWExactColors;
constexpr RGBW_MODE kRGBWBoostedWhite = RGBW_MODE::kRGBWBoostedWhite;
constexpr RGBW_MODE kRGBWMaxBrightness = RGBW_MODE::kRGBWMaxBrightness;
constexpr RGBW_MODE kRGBWColorimetric = RGBW_MODE::kRGBWColorimetric;
constexpr RGBW_MODE kRGBWColorimetricBoosted = RGBW_MODE::kRGBWColorimetricBoosted;
// kRGBWUserFunction MUST stay last — see comment on the enum in fl/gfx/rgbw.h.
constexpr RGBW_MODE kRGBWUserFunction = RGBW_MODE::kRGBWUserFunction;
