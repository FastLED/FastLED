#pragma once

#include "led_sysdefs.h"
#include "pixeltypes.h"
#include "color.h"
#include "dither_mode.h"
#include "rgbw.h"

namespace fl {

/// Common LED configuration settings
struct LEDSettings {
    CRGB correction = UncorrectedColor;
    CRGB temperature = UncorrectedTemperature;
    fl::u8 ditherMode = BINARY_DITHER;
    Rgbw rgbw = RgbwInvalid::value(); // RGBW is RGB.
};

} // namespace fl
