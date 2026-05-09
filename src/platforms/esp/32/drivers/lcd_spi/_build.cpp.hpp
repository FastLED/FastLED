// IWYU pragma: private

/// @file _build.hpp
/// @brief Unity build header for platforms\esp\32\drivers\lcd_spi/ directory

#include "platforms/esp/32/drivers/lcd_spi/channel_driver_lcd_clockless.cpp.hpp"
#include "platforms/esp/32/drivers/lcd_spi/channel_driver_lcd_spi.cpp.hpp"
#include "platforms/esp/32/drivers/lcd_spi/lcd_spi_peripheral_esp.cpp.hpp"
#include "platforms/esp/32/drivers/lcd_spi/lcd_spi_peripheral_mock.cpp.hpp"

// BusTraits<Bus::LCD_SPI> + BusTraits<Bus::LCD_CLOCKLESS> specializations.
#include "platforms/esp/32/drivers/lcd_spi/bus_traits.h"
