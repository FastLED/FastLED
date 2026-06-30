#pragma once

#include "led_sysdefs.h"  // IWYU pragma: keep
#include "pixeltypes.h"  // IWYU pragma: keep
#include "color.h"
#include "dither_mode.h"
#include "fl/channels/bus.h"
#include "rgbw.h"  // IWYU pragma: keep
#include "fl/gfx/rgbww.h"  // IWYU pragma: keep
#include "fl/stl/optional.h"
#include "fl/stl/variant.h"

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
/// **White-channel configuration** (#2558). `mWhiteCfg` is a 3-way variant:
///
///   - `fl::Empty` (default) — plain 3-channel RGB output, no white extraction.
///     Replaces the legacy `Rgbw{rgbw_mode=kRGBWInvalid}` sentinel.
///   - `Rgbw` — 4-channel RGBW output. Mode + CCT + optional shared
///     colorimetric profile handle inside the Rgbw value.
///   - `Rgbww` — 5-channel RGB + warm-W + cool-W output. Mode + two CCTs +
///     optional profile pointer inside the Rgbww value.
///
/// `setRgbw()` / `getRgbw()` on CLEDController remain backward-compatible —
/// they wrap the variant. New 5-channel callers use `setRgbww()`/`getRgbww()`.
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
    /// White-channel selection (variant): Empty = plain RGB, Rgbw = 4-channel,
    /// Rgbww = 5-channel. Default-constructed = Empty.
    fl::variant<fl::Empty, Rgbw, Rgbww> mWhiteCfg;
    Bus mBus = Bus::AUTO;              // Typed driver selection
    fl::u8 mBusWhich = 0;              // Instance selector for portable buses
    fl::optional<float> mGamma;        // Gamma correction (nullopt = use default 2.8)

    /// @return The active Rgbw if mWhiteCfg holds one, else RgbwInvalid::value().
    /// Backward-compat shim for code paths that pre-date the variant migration.
    Rgbw rgbw() const FL_NO_EXCEPT {
        if (auto* p = mWhiteCfg.ptr<Rgbw>()) return *p;
        return RgbwInvalid::value();
    }

    /// @return The active Rgbww if mWhiteCfg holds one, else RgbwwInvalid::value().
    Rgbww rgbww() const FL_NO_EXCEPT {
        if (auto* p = mWhiteCfg.ptr<Rgbww>()) return *p;
        return RgbwwInvalid::value();
    }

    /// True if this channel emits 4-channel RGBW. Requires both the Rgbw
    /// variant alternative AND an active mode — a stored
    /// `RgbwInvalid::value()` does not count, mirroring the legacy
    /// `Rgbw::active()` semantics.
    bool isRgbw() const FL_NO_EXCEPT {
        auto* p = mWhiteCfg.ptr<Rgbw>();
        return p != nullptr && p->active();
    }
    /// True if this channel emits 5-channel RGBWW. Same active-flag check as
    /// isRgbw() — a stored `RgbwwInvalid::value()` does not count.
    bool isRgbww() const FL_NO_EXCEPT {
        auto* p = mWhiteCfg.ptr<Rgbww>();
        return p != nullptr && p->active();
    }
};

} // namespace fl
