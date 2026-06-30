// IWYU pragma: private

/// @file _build.hpp
/// @brief Unity build header for platforms\esp\32\drivers\lcd_cam/ directory
///
/// LCD_RGB is never a platform default. Post-#2428 the driver impl is always
/// compiled here so that `fl::enableDriver<fl::Bus::FLEX_IO, 1>()` /
/// `FastLED.enableAllDrivers()` / `FastLED.setExclusiveDriver<fl::Bus::FLEX_IO, 1>()`
/// have the symbols they need to link. Default builds don't ODR-use any
/// symbol from this driver, so `--gc-sections` strips the whole TU.

#include "platforms/esp/32/drivers/lcd_cam/channel_driver_lcd_rgb.cpp.hpp"
#include "platforms/esp/32/drivers/lcd_cam/lcd_rgb_peripheral_esp.cpp.hpp"
#include "platforms/esp/32/drivers/lcd_cam/lcd_rgb_peripheral_mock.cpp.hpp"

// BusTraits<Bus::FLEX_IO, 1> specialization.
#include "platforms/esp/32/drivers/lcd_cam/bus_traits.h"
