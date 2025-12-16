#pragma once

#include "led_sysdefs.h"
#include "pixeltypes.h"
#include "color.h"
#include "dither_mode.h"
#include "rgbw.h"

namespace fl {

/// Optional channel configuration parameters
/// All fields have sensible defaults and can be overridden as needed
struct ChannelOptions {
    CRGB correction = UncorrectedColor;
    CRGB temperature = UncorrectedTemperature;
    fl::u8 ditherMode = BINARY_DITHER;
    Rgbw rgbw = RgbwInvalid::value(); // RGBW is RGB by default
    const char* affinity = nullptr;   // Engine affinity (nullptr = let ChannelBusManager decide)
};

} // namespace fl
