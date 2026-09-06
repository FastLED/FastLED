#pragma once

#include "fl/fled/color.h"
#include "fl/stl/int.h"

namespace fl {
namespace video {

// One decoded source sample. Storage/layout and source metadata remain
// separate: generic storage says how bytes are arranged; FLED color says what
// the values mean. P6 consumes this without an intermediate CRGB conversion.
struct PixelSample {
    fled::PixelStorage mStorage;
    fled::VideoColor mColor;
    fl::u16 mComponents[3] = {};
};

} // namespace video
} // namespace fl
