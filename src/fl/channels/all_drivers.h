#pragma once

/// @file all_drivers.h
/// @brief One-shot include + helper that enrolls every channel driver
///        available on the current platform into `ChannelManager`.
///
/// In the default build, FastLED no longer eagerly registers every driver with
/// `ChannelManager` (see issue #2428 — Phase 4 / Phase 5). Drivers are linked
/// only when their `BusTraits<fl::Bus::X>::instance()` is named, which lets
/// `--gc-sections` drop unreferenced driver TUs.
///
/// Sketches that want the 3.10.3-style behaviour (every driver registered, any
/// affinity string resolves at runtime) can opt back in with one line:
///
/// @code
///   #include "fl/channels/all_drivers.h"
///   void setup() {
///       FastLED.enableAllDrivers();      // or: fl::enableAllDrivers();
///       FastLED.add(cfg_with_affinity);  // ChannelManager picks the driver
///   }
/// @endcode
///
/// Including this header pulls in every per-driver `bus_traits.h` that is
/// reachable on the current platform, which makes the corresponding
/// `BusTraits<Bus::X>` specializations visible at the call site. The helper
/// then expands a single `enableDrivers<...>()` call covering them all.
///
/// This file is the canonical "kitchen-sink" driver bundle. Sketches that want
/// only a subset should call `fl::enableDrivers<fl::Bus::X, fl::Bus::Y>()`
/// directly after including the matching `bus_traits.h` headers.

#include "fl/channels/bus.h"
#include "fl/channels/bus_traits.h"
#include "fl/stl/compiler_control.h"
#include "fl/system/fastled.h"
#include "platforms/is_platform.h"

#if defined(FL_IS_ESP32)
// Pulled in here so the per-bus guards below see the FASTLED_ESP32_HAS_*
// macros at the call site.
#include "platforms/esp/32/feature_flags/enabled.h"  // ok platform headers // IWYU pragma: keep
#endif

// ---------------------------------------------------------------------------
// Always-on drivers
// ---------------------------------------------------------------------------

#include "platforms/shared/bitbang/bus_traits.h"  // ok platform headers // IWYU pragma: keep

// ---------------------------------------------------------------------------
// Host / native / WASM
// ---------------------------------------------------------------------------

#if defined(FL_IS_STUB) || defined(FL_IS_WASM)
#include "platforms/stub/bus_traits.h"  // ok platform headers // IWYU pragma: keep
#endif

// ---------------------------------------------------------------------------
// ESP32 family
// ---------------------------------------------------------------------------

#if defined(FL_IS_ESP32)

// Each per-driver bus_traits.h applies its own `FASTLED_ESP32_HAS_*` /
// `FASTLED_RMT5` guard internally, so the include is unconditional here.
#include "platforms/esp/32/drivers/i2s/bus_traits.h"        // ok platform headers // IWYU pragma: keep
#include "platforms/esp/32/drivers/i2s_spi/bus_traits.h"    // ok platform headers // IWYU pragma: keep
#include "platforms/esp/32/drivers/lcd_cam/bus_traits.h"    // ok platform headers // IWYU pragma: keep
#include "platforms/esp/32/drivers/lcd_spi/bus_traits.h"    // ok platform headers // IWYU pragma: keep
#include "platforms/esp/32/drivers/parlio/bus_traits.h"     // ok platform headers // IWYU pragma: keep
#include "platforms/esp/32/drivers/rmt/rmt_4/bus_traits.h"  // ok platform headers // IWYU pragma: keep
#include "platforms/esp/32/drivers/rmt/rmt_5/bus_traits.h"  // ok platform headers // IWYU pragma: keep
#include "platforms/esp/32/drivers/spi/bus_traits.h"        // ok platform headers // IWYU pragma: keep
#include "platforms/esp/32/drivers/uart/bus_traits.h"       // ok platform headers // IWYU pragma: keep

#endif  // FL_IS_ESP32

// ---------------------------------------------------------------------------
// Teensy 4.x
// ---------------------------------------------------------------------------

#if defined(FL_IS_TEENSY_4X)
#include "platforms/arm/teensy/teensy4_common/drivers/objectfled/bus_traits.h"  // ok platform headers // IWYU pragma: keep
#include "platforms/arm/teensy/teensy4_common/drivers/flexio/bus_traits.h"      // ok platform headers // IWYU pragma: keep
#endif

namespace fl {

/// @brief Register every channel driver available on this platform with
///        `ChannelManager`, restoring 3.10.3-style runtime driver selection.
///
/// This is the convenience counterpart to `fl::enableDrivers<Bus...>()`. It
/// expands to a fold over the platform-appropriate `Bus` set so that:
///
///   - Each driver's translation unit is linked (its `BusTraits<B>::instance()`
///     is ODR-used).
///   - Each driver is registered with `ChannelManager` at its default priority,
///     allowing `Channel::create(cfg)` to dispatch by affinity / priority at
///     runtime.
///
/// The helper is idempotent — `BusTraits<B>::registerWithManager()` deduplicates
/// by name on the manager side, so calling it more than once is safe.
///
/// **Performance vs. binary size.** Calling this brings every driver TU into
/// the link, which is exactly what the default build avoids (see #2428 for the
/// motivation). Only call it when you actually need runtime flexibility.
inline void enableAllDrivers() FL_NOEXCEPT {
    enableDrivers<
        Bus::BIT_BANG
#if defined(FL_IS_STUB) || defined(FL_IS_WASM)
        , Bus::STUB
#endif
#if defined(FL_IS_ESP32) && FASTLED_ESP32_HAS_RMT
        , Bus::RMT
#endif
#if defined(FL_IS_ESP32) && FASTLED_ESP32_HAS_PARLIO
        , Bus::PARLIO
#endif
#if defined(FL_IS_ESP32) && FASTLED_ESP32_HAS_CLOCKLESS_SPI
        , Bus::SPI
#endif
#if defined(FL_IS_ESP32) && FASTLED_ESP32_HAS_I2S_LCD_CAM
        , Bus::I2S
#endif
#if defined(FL_IS_ESP32) && FASTLED_ESP32_HAS_I2S
        , Bus::I2S_SPI
#endif
#if defined(FL_IS_ESP32) && FASTLED_ESP32_HAS_LCD_RGB
        , Bus::LCD_RGB
#endif
#if defined(FL_IS_ESP32) && FASTLED_ESP32_HAS_LCD_SPI
        , Bus::LCD_SPI
        , Bus::LCD_CLOCKLESS
#endif
#if defined(FL_IS_ESP32) && FASTLED_ESP32_HAS_UART
        , Bus::UART
#endif
#if defined(FL_IS_TEENSY_4X)
        , Bus::OBJECT_FLED
        , Bus::FLEX_IO
#endif
    >();
}

}  // namespace fl

/// @brief Out-of-line definition of the `CFastLED::enableAllDrivers()` member
///        declared in `FastLED.h`.
///
/// Defined here so that calling `FastLED.enableAllDrivers()` requires the user
/// to `#include "fl/channels/all_drivers.h"` — which is what brings the
/// per-platform `BusTraits<Bus::X>` specializations into scope. Without that
/// include the call fails to link, preserving the binary-bloat fix from #2428.
///
/// Marked `inline` so multiple translation units that include this header
/// don't produce duplicate-symbol linker errors.
inline void CFastLED::enableAllDrivers() {
    fl::enableAllDrivers();
}
