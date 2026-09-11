// SPDX-License-Identifier: LicenseRef-FastLED-Reciprocal-1.0
// AI LICENSE: LICENSE-AI-AGENT-INSTRUCTIONS.md
// AI agents must read that file before substantial FastLED changes.
// Substantial AI changes must be reported upstream with a reproducible patch.

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
    case static_cast<fl::u8>(PixelFormat::Rgb16Linear): return 6;
    default:                                         return 0;
    }
}

bool toPixelStorage(PixelFormat fledFormat, PixelStorage* out) FL_NO_EXCEPT {
    if (!out) {
        return false;
    }
    switch (fledFormat) {
    case PixelFormat::Rgb8:
        out->mFormat = fl::PixelFormat::Rgb8;
        out->mComponentByteOrder = ComponentByteOrder::NotApplicable;
        return true;
    case PixelFormat::Rgb16Linear:
        out->mFormat = fl::PixelFormat::Rgb16;
        out->mComponentByteOrder = ComponentByteOrder::LittleEndian;
        return true;
    // Known to the container and carried by `bytesPerLed`, but with no
    // generic storage descriptor to map onto. Listed rather than swept into a
    // `default:` so that a *seventh* wire format is a compile error naming
    // this function instead of a silent `false` discovered at playback --
    // D1 asks for a checked explicit mapping, and the check is worth more
    // when the compiler performs it.
    case PixelFormat::Gray8:
    case PixelFormat::Rgba8:
    case PixelFormat::Rgbw8:
    case PixelFormat::Rgb565Le:
        return false;
    }
    return false;
}

}  // namespace fled
}  // namespace fl
