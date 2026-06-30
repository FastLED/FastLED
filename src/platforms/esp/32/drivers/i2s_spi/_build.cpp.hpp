// IWYU pragma: private

/// @file _build.hpp
/// @brief Unity build header for platforms\esp\32\drivers\i2s_spi/ directory
///
/// I2S_SPI is the platform-default true-SPI driver on ESP32dev (original
/// ESP32). On dev the legacy `addLeds<APA102>` path pre-binds to
/// `BusTraits<Bus::FLEX_IO, 0>::instancePtr()` which links this TU. The impl
/// files self-gate on the appropriate hardware / mock macros, so we include
/// them unconditionally here (host unit tests rely on the mock peripheral
/// being available) and let `--gc-sections` strip the unused TU on
/// platforms that don't ODR-use the singleton.

#include "platforms/esp/32/drivers/i2s_spi/channel_driver_i2s_spi.cpp.hpp"
#include "platforms/esp/32/drivers/i2s_spi/i2s_spi_peripheral_esp.cpp.hpp"
#include "platforms/esp/32/drivers/i2s_spi/i2s_spi_peripheral_mock.cpp.hpp"

// BusTraits<Bus::FLEX_IO, 0> specialization.
#include "platforms/esp/32/drivers/i2s_spi/bus_traits.h"
