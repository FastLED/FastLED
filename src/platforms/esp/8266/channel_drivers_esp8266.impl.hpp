#pragma once

// IWYU pragma: private

/// @file channel_drivers_esp8266.impl.hpp
/// @brief ESP8266 channel-driver fragment for `fl::enableAllDrivers()`.
///
/// ESP8266 ships two channel-API drivers today:
///   - the shared portable GPIO bit-bang driver (`platforms/shared/bitbang/`,
///     `Bus::BIT_BANG`), which covers both clockless and SPI chipsets and is the
///     target of `DefaultBus<ClocklessChipset>` / `DefaultBus<SpiChipsetConfig>`
///     (see `src/fl/channels/bus.h`);
///   - the register-level hardware HSPI driver (`Bus::SPI`), which drives the
///     fixed GPIO13/14 pair (`platforms/esp/8266/hspi_bus_traits.h`).
///
/// Further native engines (I2S, UART — #4943 / #4944) register here as their
/// `BusTraits<Bus::X>` specializations land.
///
/// Tree-shaken when no sketch references `FastLED.enableAllDrivers()` — see
/// `src/platforms/channel_drivers.impl.cpp.hpp` for the dispatch contract.

#include "fl/channels/bus.h"
#include "fl/channels/bus_traits.h"
#include "fl/stl/compiler_control.h"
#include "platforms/esp/8266/hspi_bus_traits.h"      // ok platform headers // IWYU pragma: keep
#include "platforms/esp/8266/i2s_bus_traits.h"       // ok platform headers // IWYU pragma: keep
#include "platforms/shared/bitbang/bus_traits.h"     // ok platform headers // IWYU pragma: keep

namespace fl {
namespace platforms {

inline void enableAllChannelDrivers() FL_NO_EXCEPT {
    fl::enableDriver<fl::Bus::BIT_BANG>();
    fl::enableDriver<fl::Bus::SPI>();
    fl::enableDriver<fl::Bus::FLEX_IO>();  // the ESP8266 "I2S" engine
}

}  // namespace platforms
}  // namespace fl
