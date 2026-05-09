// IWYU pragma: private

/// @file _build.hpp
/// @brief Unity build header for platforms\esp\32\drivers\i2s_spi/ directory

#include "platforms/esp/32/drivers/i2s_spi/channel_driver_i2s_spi.cpp.hpp"
#include "platforms/esp/32/drivers/i2s_spi/i2s_spi_peripheral_esp.cpp.hpp"
#include "platforms/esp/32/drivers/i2s_spi/i2s_spi_peripheral_mock.cpp.hpp"

// BusTraits<Bus::I2S_SPI> specialization.
#include "platforms/esp/32/drivers/i2s_spi/bus_traits.h"
