// IWYU pragma: private

/// @file _build.hpp
/// @brief Unity build header for platforms\esp\32\drivers\lcd_spi/ directory
///
/// LCD_SPI / LCD_CLOCKLESS use the LCD_CAM peripheral and are the platform
/// default true-SPI drivers on ESP32-S3. On S3 the legacy `addLeds<APA102>`
/// path pre-binds to `BusTraits<Bus::FLEX_IO, 0>::instancePtr()` which links
/// this TU. The impl files self-gate on the appropriate hardware / mock
/// macros, so we include them unconditionally here (host unit tests rely on
/// the mock peripheral being available) and let `--gc-sections` strip the
/// unused TU on platforms that don't ODR-use the singleton.

#include "platforms/esp/32/drivers/lcd_spi/channel_driver_lcd_clockless.cpp.hpp"
#include "platforms/esp/32/drivers/lcd_spi/channel_driver_lcd_spi.cpp.hpp"
#include "platforms/esp/32/drivers/lcd_spi/lcd_spi_peripheral_esp.cpp.hpp"
#include "platforms/esp/32/drivers/lcd_spi/lcd_spi_peripheral_mock.cpp.hpp"

// BusTraits<Bus::FLEX_IO, 0> specialization for LCD_SPI + LCD_CLOCKLESS.
#include "platforms/esp/32/drivers/lcd_spi/bus_traits.h"
