#pragma once

// Pixel-format enum + bytes-per-LED lookup for the .fled v1 container.
// See FLED_FORMAT.md "Pixel Format Enum" section for the canonical table.
// Values are the on-disk byte at header offset 5.

#include "fl/stl/int.h"
#include "fl/stl/noexcept.h"

namespace fl {
namespace fled {

enum class PixelFormat : fl::u8 {
    Rgb8     = 0x00,  // 3 bpp - R, G, B
    Gray8    = 0x01,  // 1 bpp - Y
    Rgba8    = 0x02,  // 4 bpp - R, G, B, A
    Rgbw8    = 0x03,  // 4 bpp - R, G, B, W
    Rgb565Le = 0x04,  // 2 bpp - little-endian RGB565
};

// Returns the bytes-per-LED for a v1 pixel-format byte.
// Returns 0 for unknown / reserved formats (0x05 - 0xff) - callers should
// treat 0 as "consumer does not support this pixel_format" and reject
// before reading frame bytes per FLED_FORMAT.md.
fl::u8 bytesPerLed(fl::u8 pixelFormat) FL_NO_EXCEPT;

inline fl::u8 bytesPerLed(PixelFormat pf) FL_NO_EXCEPT {
    return bytesPerLed(static_cast<fl::u8>(pf));
}

}  // namespace fled
}  // namespace fl
