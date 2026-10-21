#pragma once

#include "color.h"
#include "dither_mode.h"
#include "pixeltypes.h"
#include "rgbw.h"
#include "fl/gfx/rgbww.h"
#include "fl/stl/variant.h"

namespace fl {

// Only settings consumed by the traditional controller/encoder path.
// ChannelOptions must never be stored in the legacy controller base.
struct LegacySettings {
    CRGB mCorrection = UncorrectedColor;
    CRGB mTemperature = UncorrectedTemperature;
    u8 mDitherMode = BINARY_DITHER;
    variant<Empty, Rgbw, Rgbww> mWhiteCfg;

    Rgbw rgbw() const FL_NO_EXCEPT {
        if (const auto* value = mWhiteCfg.ptr<Rgbw>()) return *value;
        return RgbwInvalid::value();
    }
    Rgbww rgbww() const FL_NO_EXCEPT {
        if (const auto* value = mWhiteCfg.ptr<Rgbww>()) return *value;
        return RgbwwInvalid::value();
    }
};

} // namespace fl
