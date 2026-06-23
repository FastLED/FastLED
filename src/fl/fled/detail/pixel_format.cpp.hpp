// ok no header - implementation for fl/fled/detail/pixel_format.h.

#include "fl/fled/detail/pixel_format.h"

namespace fl {
namespace fled {

fl::u8 bytesPerLed(fl::u8 pixelFormat) FL_NO_EXCEPT {
    switch (pixelFormat) {
    case static_cast<fl::u8>(PixelFormat::Rgb8):     return 3;
    case static_cast<fl::u8>(PixelFormat::Gray8):    return 1;
    case static_cast<fl::u8>(PixelFormat::Rgba8):    return 4;
    case static_cast<fl::u8>(PixelFormat::Rgbw8):    return 4;
    case static_cast<fl::u8>(PixelFormat::Rgb565Le): return 2;
    default:                                         return 0;
    }
}

}  // namespace fled
}  // namespace fl
