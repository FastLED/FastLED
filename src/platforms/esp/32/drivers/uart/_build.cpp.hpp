// IWYU pragma: private

/// @file _build.hpp
/// @brief Unity build header for platforms\esp\32\drivers\uart/ directory
///
/// UART is never a platform default. Post-#2428 the driver impl is always
/// compiled here so that `fl::enableDrivers<fl::Bus::UART>()` /
/// `FastLED.enableAllDrivers()` / `FastLED.setExclusiveDriver<fl::Bus::UART>()`
/// have the symbols they need to link. Default builds don't ODR-use any
/// symbol from this driver, so `--gc-sections` strips the whole TU.

#include "platforms/esp/32/drivers/uart/channel_driver_uart.cpp.hpp"
#include "platforms/esp/32/drivers/uart/uart_peripheral_esp.cpp.hpp"
// The waveN encoder implementation is platform-neutral and compiled by
// fl/channels/_build.cpp.hpp (fl/channels/uart_wave_encoder.cpp.hpp).

// BusTraits<Bus::UART> specialization.
#include "platforms/esp/32/drivers/uart/bus_traits.h"
