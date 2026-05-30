#pragma once

/// @file rgbww_pixel.h
/// @brief Raw 5-channel RGBWW pixel data structure for encoders
///
/// Mirrors RGBWPixel; consumed by encoder adapters that produce 5-byte
/// output for RGB + warm-white + cool-white strips. Kept separate from
/// rgbww.h to avoid pulling EOrderWW / RGBWW_MODE into encoder TUs that
/// only need the raw byte container.

#include "fl/stl/stdint.h"
#include "fl/stl/noexcept.h"

namespace fl {

/// @brief Simple 5-channel pixel struct: red, green, blue, warm-white, cool-white.
/// @note This represents raw RGBWW pixel data (5 bytes per pixel).
/// @note Not to be confused with `Rgbww` which is a configuration struct.
struct RGBWWPixel {
    u8 r, g, b, ww, wc;

    RGBWWPixel() FL_NOEXCEPT : r(0), g(0), b(0), ww(0), wc(0) {}
    RGBWWPixel(u8 _r, u8 _g, u8 _b, u8 _ww, u8 _wc) FL_NOEXCEPT
        : r(_r), g(_g), b(_b), ww(_ww), wc(_wc) {}
};

} // namespace fl
