// IWYU pragma: private

/// @file _build.hpp
/// @brief Unity build header for platforms\esp\32\drivers\spi/ directory
///
/// `Bus::SPI` is the *clockless-over-SPI* driver (WS2812 etc. over the SPI
/// peripheral, using wave8 encoding). It is never a platform default
/// (true-SPI chipsets like APA102 / SK9822 / HD108 go through `Bus::FLEX_IO`
/// slot 0 on ESP32dev/S3 when those parallel-SPI engines are selected).
///
/// Post-#2428 the driver impl is always compiled here so that
/// `fl::enableDrivers<fl::Bus::SPI>()` / `FastLED.enableAllDrivers()` /
/// `FastLED.setExclusiveDriver<fl::Bus::SPI>()` have the symbols they need
/// to link. Default builds don't ODR-use any symbol from this driver, so
/// `--gc-sections` strips the whole TU.

#include "platforms/esp/32/drivers/spi/channel_driver_spi.cpp.hpp"
#include "platforms/esp/32/drivers/spi/spi_esp32_init.cpp.hpp"
#include "platforms/esp/32/drivers/spi/spi_hw_1_esp32.cpp.hpp"
#include "platforms/esp/32/drivers/spi/spi_peripheral_esp.cpp.hpp"
#include "platforms/esp/32/drivers/spi/spi_platform_esp32.cpp.hpp"

// BusTraits<Bus::SPI> specialization.
#include "platforms/esp/32/drivers/spi/bus_traits.h"
