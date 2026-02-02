/// @file _build.hpp
/// @brief Unity build header for platforms\esp\32\drivers\spi/ directory
/// Includes all implementation files in alphabetical order
///
/// Note: wave8_encoder_spi files renamed to .disabled for potential future
/// multi-lane support. Uses Espressif 3-bit encoding instead.

#include "platforms/esp/32/drivers/spi/channel_engine_spi.cpp.hpp"
#include "platforms/esp/32/drivers/spi/spi_esp32_init.cpp.hpp"
#include "platforms/esp/32/drivers/spi/spi_hw_1_esp32.cpp.hpp"
#include "platforms/esp/32/drivers/spi/spi_peripheral_esp.cpp.hpp"
#include "platforms/esp/32/drivers/spi/spi_platform_esp32.cpp.hpp"
