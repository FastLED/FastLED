/// @file clockless_objectfled.h
/// @brief Rectangular-frame sizing helpers shared by the Teensy 4.x
///        ObjectFLED channel engine and its tests.
///
/// The legacy `addLeds<>()` ObjectFLED proxy (ObjectFLEDGroup /
/// ObjectFLEDRegistry / ClocklessController_ObjectFLED_Proxy) that used to
/// live here was a second, independent ObjectFLED implementation. Legacy
/// strips now go through `fl::SlimBridgeController` onto
/// `ChannelEngineObjectFLED` (see `clockless.h`, issue #4588).

#pragma once

// IWYU pragma: private

#include "fl/stl/int.h"
#include "fl/stl/noexcept.h"

namespace fl {

inline u32 objectFledBytesPerLed(bool isRgbw) FL_NO_EXCEPT {
    return isRgbw ? 4u : 3u;
}

inline u32 objectFledLedsPerStripForRectangularBytes(
        u32 bytesPerStrip, bool isRgbw) FL_NO_EXCEPT {
    const u32 bytesPerLed = objectFledBytesPerLed(isRgbw);
    if (bytesPerStrip == 0) {
        return 0;
    }
    return (bytesPerStrip + bytesPerLed - 1u) / bytesPerLed;
}

inline u32 objectFledTotalLedsForRectangularBlock(
        u32 numStrips, u32 bytesPerStrip, bool isRgbw) FL_NO_EXCEPT {
    return numStrips *
           objectFledLedsPerStripForRectangularBytes(bytesPerStrip, isRgbw);
}

inline u32 objectFledFrameBytesForRectangularBlock(
        u32 numStrips, u32 bytesPerStrip, bool isRgbw) FL_NO_EXCEPT {
    return objectFledTotalLedsForRectangularBlock(
               numStrips, bytesPerStrip, isRgbw) *
           objectFledBytesPerLed(isRgbw);
}

}  // namespace fl
