#pragma once

// IWYU pragma: private

/// @file channel_drivers_esp8266.impl.hpp
/// @brief ESP8266 channel-driver fragment for `fl::enableAllDrivers()`.
///
/// ESP8266 ships exactly one channel-API driver today: the shared portable
/// GPIO bit-bang driver (`platforms/shared/bitbang/`, `Bus::BIT_BANG`). It
/// covers both clockless and SPI chipsets, and is also the target of
/// `DefaultBus<ClocklessChipset>` / `DefaultBus<SpiChipsetConfig>` (see
/// `src/fl/channels/bus.h`). Further native engines (I2S, HSPI, UART — #4942 /
/// #4943 / #4944) register here as their `BusTraits<Bus::X>` specializations
/// land.
///
/// Tree-shaken when no sketch references `FastLED.enableAllDrivers()` — see
/// `src/platforms/channel_drivers.impl.cpp.hpp` for the dispatch contract.

#include "fl/channels/bus.h"
#include "fl/channels/bus_traits.h"
#include "fl/stl/compiler_control.h"
#include "platforms/shared/bitbang/bus_traits.h"  // ok platform headers // IWYU pragma: keep

namespace fl {
namespace platforms {

inline void enableAllChannelDrivers() FL_NO_EXCEPT {
    fl::enableDriver<fl::Bus::BIT_BANG>();
}

}  // namespace platforms
}  // namespace fl
