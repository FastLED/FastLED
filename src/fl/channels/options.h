#pragma once

#include "led_sysdefs.h"  // IWYU pragma: keep
#include "pixeltypes.h"  // IWYU pragma: keep
#include "color.h"
#include "dither_mode.h"
#include "rgbw.h"  // IWYU pragma: keep
#include "fl/stl/string.h"  // IWYU pragma: keep
#include "fl/stl/optional.h"

namespace fl {

/// Optional channel configuration parameters
/// All fields have sensible defaults and can be overridden as needed
struct ChannelOptions {
    CRGB mCorrection = UncorrectedColor;
    CRGB mTemperature = UncorrectedTemperature;
    fl::u8 mDitherMode = BINARY_DITHER;
    Rgbw mRgbw = RgbwInvalid::value(); // RGBW is RGB by default
    fl::string mAffinity;              // Engine affinity (empty = let ChannelManager decide)
    fl::optional<float> mGamma;        // Gamma correction (nullopt = use default 2.8)
};

} // namespace fl
