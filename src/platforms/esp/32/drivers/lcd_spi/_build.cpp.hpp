// IWYU pragma: private

/// @file _build.hpp
/// @brief Unity build header for platforms\esp\32\drivers\lcd_spi/ directory
///
/// Phase 5c of #2428: LCD_SPI is the platform-default true-SPI driver on
/// ESP32-S3. On S3 the driver TU is always compiled (legacy
/// `addLeds<APA102, ...>` pre-binds to it). On all other ESP32 variants
/// LCD_SPI is an alternate driver, gated by
/// `FASTLED_DISABLE_LEGACY_DRIVER_REGISTRY`.

#include "fl/channels/bus.h"  // brings in FASTLED_DISABLE_LEGACY_DRIVER_REGISTRY
#include "platforms/is_platform.h"

#if !FASTLED_DISABLE_LEGACY_DRIVER_REGISTRY || defined(FL_IS_ESP_32S3)

#include "platforms/esp/32/drivers/lcd_spi/channel_driver_lcd_clockless.cpp.hpp"
#include "platforms/esp/32/drivers/lcd_spi/channel_driver_lcd_spi.cpp.hpp"
#include "platforms/esp/32/drivers/lcd_spi/lcd_spi_peripheral_esp.cpp.hpp"
#include "platforms/esp/32/drivers/lcd_spi/lcd_spi_peripheral_mock.cpp.hpp"

// BusTraits<Bus::LCD_SPI> + BusTraits<Bus::LCD_CLOCKLESS> specializations.
#include "platforms/esp/32/drivers/lcd_spi/bus_traits.h"

#endif  // legacy mode OR ESP32-S3 (default LCD_SPI platform)
