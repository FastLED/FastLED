#pragma once

// Public pixel-format boundary for .fled v1 containers. PixelFormat owns
// stable serialized wire identifiers; fl::PixelFormat describes generic
// storage and deliberately has a separate numeric domain.

#include "fl/codec/pixel.h"
#include "fl/stl/int.h"
#include "fl/stl/noexcept.h"

namespace fl {
namespace fled {

struct VideoColor;

enum class PixelFormat : fl::u8 {
    Rgb8        = 0x00,  // 3 bpp - R, G, B
    Gray8       = 0x01,  // 1 bpp - Y
    Rgba8       = 0x02,  // 4 bpp - R, G, B, A
    Rgbw8       = 0x03,  // 4 bpp - R, G, B, W
    Rgb565Le    = 0x04,  // 2 bpp - little-endian RGB565
    Rgb16Linear = 0x05,  // 6 bpp - R, G, B as u16le, linear light
};

// The component byte order travels with generic storage. In particular,
// Rgb16 alone never authorizes treating FLED's u16le payload as native words.
enum class ComponentByteOrder : fl::u8 {
    NotApplicable = 0,
    LittleEndian,
    Native,
};

struct PixelStorage {
    fl::PixelFormat mFormat;
    ComponentByteOrder mComponentByteOrder;
};

// Returns bytes per LED for a v1 pixel-format byte, or zero when the value is
// unknown/reserved. Callers must reject zero before reading payload bytes.
fl::u8 bytesPerLed(fl::u8 pixelFormat) FL_NO_EXCEPT;

inline fl::u8 bytesPerLed(PixelFormat pf) FL_NO_EXCEPT {
    return bytesPerLed(static_cast<fl::u8>(pf));
}

// Checked conversion between FLED's wire enum and generic storage. Only
// direct RGB storage has a mapping today. The source color declaration is
// resolved separately with resolveVideoColor() and must stay paired with this
// descriptor through playback or serialization.
bool toPixelStorage(PixelFormat fledFormat, PixelStorage* out) FL_NO_EXCEPT;
bool toFledPixelFormat(const PixelStorage& storage, const VideoColor& color,
                       PixelFormat* out) FL_NO_EXCEPT;

}  // namespace fled
}  // namespace fl
