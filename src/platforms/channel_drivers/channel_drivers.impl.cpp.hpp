// IWYU pragma: private

/// @file platforms/channel_drivers.impl.cpp.hpp
/// @brief Platform dispatch for the `fl::enableAllDrivers()` driver bundle.
///
/// This router is the only place that needs the per-platform `BusTraits<Bus::X>`
/// specializations visible. It includes exactly one platform fragment, so every
/// per-driver `bus_traits.h` is pulled in here, not at the user's call site.
/// Platforms without an explicit fragment use the null implementation, which
/// gives the public API an empty definition without registering any drivers.
///
/// Tree-shaking: when a sketch never calls `FastLED.enableAllDrivers()` or
/// `fl::enableAllDrivers()`, `-Wl,--gc-sections` drops this TU's text section,
/// which transitively drops every `BusTraits<B>::registerWithManager()`,
/// `BusTraits<B>::instancePtr()`, and each platform driver's factory
/// (`ChannelEngineRMT::create()`, `createI2sEngine()`, ...). The
/// binary-size fix from #2428 is preserved structurally rather than by
/// convention - no "magic include" required at the call site.

#include "fl/channels/all_drivers.h"

#include "platforms/is_platform.h"

#if defined(FL_IS_ESP32)
#include "platforms/esp/32/channel_drivers_esp32.impl.hpp"
#elif defined(FL_IS_TEENSY_4X)
#include "platforms/arm/teensy/channel_drivers_teensy.impl.hpp"
#elif defined(FL_IS_ARM_LPC)
#include "platforms/arm/lpc/channel_drivers_lpc.impl.hpp"
#elif defined(FL_IS_STUB) || defined(FL_IS_WASM)
#include "platforms/stub/channel_drivers_stub.impl.hpp"
#else
#include "platforms/shared/channel_drivers_null.impl.hpp"
#endif

namespace fl {

void enableAllDrivers() FL_NO_EXCEPT {
    platforms::enableAllChannelDrivers();
}

}  // namespace fl
