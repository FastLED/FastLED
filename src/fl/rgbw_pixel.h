#pragma once

/// @file rgbw_pixel.h
/// @brief Simple RGBW pixel data structure for encoders
///
/// This file defines the RGBWPixel struct used by encoders to represent
/// 4-channel RGBW pixel data. Kept separate from rgbw.h to avoid circular
/// dependencies.

#include "fl/stl/stdint.h"

namespace fl {

/// @brief Simple RGBW pixel struct for encoder input
/// @note This represents raw RGBW pixel data (r, g, b, w bytes)
/// @note Not to be confused with `Rgbw` which is a configuration struct
struct RGBWPixel {
    u8 r, g, b, w;

    RGBWPixel() : r(0), g(0), b(0), w(0) {}
    RGBWPixel(u8 _r, u8 _g, u8 _b, u8 _w) : r(_r), g(_g), b(_b), w(_w) {}
};

}  // namespace fl
