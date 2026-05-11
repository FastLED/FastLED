// IWYU pragma: private

/// @file _build.hpp
/// @brief Unity build header for platforms\esp\32\drivers\spi/ directory
/// Includes all implementation files in alphabetical order
///
/// Phase 5c of #2428: the clockless-over-SPI driver is never a platform
/// default. When the user opts in to `FASTLED_DISABLE_LEGACY_DRIVER_REGISTRY=1`,
/// this TU becomes empty and the SPI clockless driver is dropped from the
/// binary.
///
/// NOTE: This is `Bus::SPI`, the *clockless* SPI driver (WS2812 etc. over the
/// SPI peripheral). True-SPI chipsets (APA102, SK9822, HD108) go through
/// `Bus::I2S_SPI` on ESP32dev or `Bus::LCD_SPI` on ESP32-S3.

#include "fl/channels/bus.h"  // brings in FASTLED_DISABLE_LEGACY_DRIVER_REGISTRY

#if !FASTLED_DISABLE_LEGACY_DRIVER_REGISTRY

#include "platforms/esp/32/drivers/spi/channel_driver_spi.cpp.hpp"
#include "platforms/esp/32/drivers/spi/spi_esp32_init.cpp.hpp"
#include "platforms/esp/32/drivers/spi/spi_hw_1_esp32.cpp.hpp"
#include "platforms/esp/32/drivers/spi/spi_peripheral_esp.cpp.hpp"
#include "platforms/esp/32/drivers/spi/spi_platform_esp32.cpp.hpp"

// BusTraits<Bus::SPI> specialization.
#include "platforms/esp/32/drivers/spi/bus_traits.h"

#endif  // !FASTLED_DISABLE_LEGACY_DRIVER_REGISTRY
