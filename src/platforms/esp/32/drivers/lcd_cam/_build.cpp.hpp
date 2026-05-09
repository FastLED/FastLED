// IWYU pragma: private

/// @file _build.hpp
/// @brief Unity build header for platforms\esp\32\drivers\lcd_cam/ directory
/// Includes all implementation files in alphabetical order

#include "platforms/esp/32/drivers/lcd_cam/channel_driver_lcd_rgb.cpp.hpp"
#include "platforms/esp/32/drivers/lcd_cam/lcd_rgb_peripheral_esp.cpp.hpp"
#include "platforms/esp/32/drivers/lcd_cam/lcd_rgb_peripheral_mock.cpp.hpp"

// BusTraits<Bus::LCD_RGB> specialization.
#include "platforms/esp/32/drivers/lcd_cam/bus_traits.h"
