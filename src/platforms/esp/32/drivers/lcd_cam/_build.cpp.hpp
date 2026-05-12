// IWYU pragma: private

/// @file _build.hpp
/// @brief Unity build header for platforms\esp\32\drivers\lcd_cam/ directory
///
/// LCD_RGB is never a platform default. Post-#2428 the driver impl is always
/// compiled here so that `fl::enableDrivers<fl::Bus::LCD_RGB>()` /
/// `FastLED.enableAllDrivers()` / `FastLED.setExclusiveDriver<fl::Bus::LCD_RGB>()`
/// have the symbols they need to link. Default builds don't ODR-use any
/// symbol from this driver, so `--gc-sections` strips the whole TU.

#include "platforms/esp/32/drivers/lcd_cam/channel_driver_lcd_rgb.cpp.hpp"
#include "platforms/esp/32/drivers/lcd_cam/lcd_rgb_peripheral_esp.cpp.hpp"
#include "platforms/esp/32/drivers/lcd_cam/lcd_rgb_peripheral_mock.cpp.hpp"

// BusTraits<Bus::LCD_RGB> specialization.
#include "platforms/esp/32/drivers/lcd_cam/bus_traits.h"
