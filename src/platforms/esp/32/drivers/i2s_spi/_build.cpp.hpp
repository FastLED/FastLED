// IWYU pragma: private

/// @file _build.hpp
/// @brief Unity build header for platforms\esp\32\drivers\i2s_spi/ directory
///
/// Phase 5c of #2428: I2S_SPI is the platform-default true-SPI driver on
/// ESP32dev (original ESP32). On dev the driver TU is always compiled
/// (legacy `addLeds<APA102, ...>` pre-binds to it). On all other ESP32
/// variants I2S_SPI is an alternate driver, gated by
/// `FASTLED_DISABLE_LEGACY_DRIVER_REGISTRY`.

#include "fl/channels/bus.h"  // brings in FASTLED_DISABLE_LEGACY_DRIVER_REGISTRY
#include "platforms/is_platform.h"

#if !FASTLED_DISABLE_LEGACY_DRIVER_REGISTRY || defined(FL_IS_ESP_32DEV)

#include "platforms/esp/32/drivers/i2s_spi/channel_driver_i2s_spi.cpp.hpp"
#include "platforms/esp/32/drivers/i2s_spi/i2s_spi_peripheral_esp.cpp.hpp"
#include "platforms/esp/32/drivers/i2s_spi/i2s_spi_peripheral_mock.cpp.hpp"

// BusTraits<Bus::I2S_SPI> specialization.
#include "platforms/esp/32/drivers/i2s_spi/bus_traits.h"

#endif  // legacy mode OR ESP32dev (default I2S_SPI platform)
