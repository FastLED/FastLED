#pragma once

#include "fl/int.h"
namespace fl {

// Color formats for decoded output
enum class PixelFormat {
    RGB565,     // 16-bit RGB: RRRRR GGGGGG BBBBB
    RGB888,     // 24-bit RGB: RRRRRRRR GGGGGGGG BBBBBBBB
    RGBA8888,   // 32-bit RGBA: RRRRRRRR GGGGGGGG BBBBBBBB AAAAAAAA
    YUV420      // YUV 4:2:0 format (mainly for internal use)
};

// Calculate bytes per pixel for given format
inline fl::u8 getBytesPerPixel(PixelFormat format) {
    switch (format) {
        case PixelFormat::RGB565: return 2;
        case PixelFormat::RGB888: return 3;
        case PixelFormat::RGBA8888: return 4;
        case PixelFormat::YUV420: return 1; // Simplified for luminance component
        default: return 3;
    }
}

// Convert RGB565 to RGB888 with proper scaling to full 8-bit range using lookup tables
void rgb565ToRgb888(fl::u16 rgb565, fl::u8& r, fl::u8& g, fl::u8& b);

// Convert RGB888 to RGB565
inline fl::u16 rgb888ToRgb565(fl::u8 r, fl::u8 g, fl::u8 b) {
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}

} // namespace fl