#pragma once

#include "led_sysdefs.h"  // IWYU pragma: keep
#include "pixeltypes.h"  // IWYU pragma: keep
#include "color.h"
#include "dither_mode.h"
#include "fl/channels/bus.h"
#include "rgbw.h"  // IWYU pragma: keep
#include "fl/stl/optional.h"

namespace fl {

/// Optional channel configuration parameters
/// All fields have sensible defaults and can be overridden as needed.
///
/// **Driver selection** (#2459). `mBus` is the single typed selector:
///
///   - `Bus::AUTO` (default) — let `ChannelManager` pick by priority dispatch.
///   - Any other value — pin this channel to the named driver via
///     `ChannelManager::findDriverByName(busName(mBus))`. If the named driver
///     isn't registered, `Channel::showPixels` emits a one-shot `FL_ERROR`
///     with the resolution hint (`fl::enableDrivers<fl::Bus::X>()` or
///     `FastLED.enableAllDrivers()`) and falls back to AUTO/priority.
///
/// **Custom / third-party / mock drivers** whose names aren't in the
/// `fl::Bus` enum should be bound either by priority (clear competing
/// drivers via `clearAllDrivers()` and let the mock win priority dispatch)
/// or via `ChannelManager::setExclusiveDriverByName(name)` for process-wide
/// binding. There is no string-typed affinity field on `ChannelOptions`.
struct ChannelOptions {
    CRGB mCorrection = UncorrectedColor;
    CRGB mTemperature = UncorrectedTemperature;
    fl::u8 mDitherMode = BINARY_DITHER;
    Rgbw mRgbw = RgbwInvalid::value(); // RGBW is RGB by default
    Bus mBus = Bus::AUTO;              // Typed driver selection
    fl::optional<float> mGamma;        // Gamma correction (nullopt = use default 2.8)
};

} // namespace fl
