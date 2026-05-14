/// @file all_drivers.cpp.hpp
/// @brief Out-of-line definitions for `fl::enableAllDrivers()` and the
///        `CFastLED::enableAllDrivers()` member.
///
/// This translation unit is the ONLY place that needs the per-platform
/// `BusTraits<Bus::X>` specializations visible — so every per-driver
/// `bus_traits.h` is pulled in here, not at the user's call site.
///
/// Tree-shaking: when a sketch never calls `FastLED.enableAllDrivers()` or
/// `fl::enableAllDrivers()`, `-Wl,--gc-sections` drops this TU's text section,
/// which transitively drops every `BusTraits<B>::registerWithManager()`,
/// `BusTraits<B>::instancePtr()`, and each platform driver's factory
/// (`ChannelEngineRMT::create()`, `createI2sEngine()`, ...). The
/// binary-size fix from #2428 is preserved structurally rather than by
/// convention — no "magic include" required at the call site.

#include "fl/channels/all_drivers.h"

#include "FastLED.h"   // CFastLED class (for the out-of-line member definition)
#include "fl/channels/bus.h"
#include "fl/channels/bus_traits.h"
#include "fl/stl/compiler_control.h"
#include "platforms/is_platform.h"

#if defined(FL_IS_ESP32)
// Pulled in here so the per-bus guards below see the FASTLED_ESP32_HAS_*
// macros at the point of instantiation.
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

void enableAllDrivers() FL_NOEXCEPT {
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

void CFastLED::enableAllDrivers() {
    fl::enableAllDrivers();
}
