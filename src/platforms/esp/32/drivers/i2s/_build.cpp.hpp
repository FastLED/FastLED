// IWYU pragma: private

/// @file _build.hpp
/// @brief Unity build header for platforms\esp\32\drivers\i2s/ directory
///
/// I2S (LCD_CAM clockless on ESP32-S3) is never a platform default. Post-#2428
/// the driver impl is always compiled here so that
/// `fl::enableDriver<fl::Bus::FLEX_IO, 0>()` / `FastLED.enableAllDrivers()` /
/// `FastLED.setExclusiveDriver<fl::Bus::FLEX_IO, 0>()` have the symbols they need to
/// link. Default builds don't ODR-use any symbol from this driver, so
/// `--gc-sections` strips the whole TU.

#include "platforms/esp/32/drivers/i2s/channel_driver_i2s.cpp.hpp"
#include "platforms/esp/32/drivers/i2s/clockless_i2s_esp32.cpp.hpp"

#include "platforms/esp/32/drivers/i2s/i2s_esp32dev.cpp.hpp"
#include "platforms/esp/32/drivers/i2s/i2s_lcd_cam_peripheral_esp.cpp.hpp"
#include "platforms/esp/32/drivers/i2s/i2s_lcd_cam_peripheral_mock.cpp.hpp"

#include "platforms/esp/32/drivers/i2s/wave8_encoder_i2s.cpp.hpp"

// Legacy concrete I2S driver implementation.
#include "platforms/esp/32/drivers/i2s/bus_traits.h"
