#pragma once

/// @file all_drivers.h
/// @brief Declaration of `fl::enableAllDrivers()` — enrolls every channel
///        driver available on the current platform into `ChannelManager`.
///
/// The definition lives out-of-line in
/// `platforms/channel_drivers.impl.cpp.hpp` (linked into `libfastled`), so
/// calling `FastLED.enableAllDrivers()` or `fl::enableAllDrivers()` no longer
/// requires any "magic include" at the call site — `<FastLED.h>` is enough.
///
/// **Tree-shaking.** `-Wl,--gc-sections` (the default for every embedded
/// toolchain FastLED supports) drops the `enableAllDrivers()` TU when no
/// sketch references it. That transitively drops every
/// `BusTraits<B>::registerWithManager()` / `instancePtr()` and each platform
/// driver's factory — the binary-size fix from #2428 is preserved
/// structurally rather than by convention. Including this header is
/// harmless but no longer required.

#include "fl/stl/compiler_control.h"

namespace fl {

/// @brief Register every channel driver available on this platform with
///        `ChannelManager`, restoring 3.10.3-style runtime driver selection.
///
/// Convenience counterpart to `fl::enableDrivers<Bus...>()`. Idempotent —
/// `BusTraits<B>::registerWithManager()` deduplicates by name on the manager
/// side, so calling more than once is safe.
///
/// **Performance vs. binary size.** Calling this brings every driver TU
/// into the link, which is exactly what the default build avoids (see
/// #2428). Only call when you actually need runtime driver flexibility.
void enableAllDrivers() FL_NOEXCEPT;

}  // namespace fl
