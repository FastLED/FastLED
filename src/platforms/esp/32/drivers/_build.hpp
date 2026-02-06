/// @file _build.hpp
/// @brief Unity build header for platforms\esp\32\drivers/ directory
/// Includes all implementation files in alphabetical order

// Root directory implementations (alphabetical order)
#include "platforms/esp/32/drivers/channel_bus_manager_esp32.cpp.hpp"
#include "platforms/esp/32/drivers/cled.cpp.hpp"
#include "platforms/esp/32/drivers/spi_hw_manager_esp32.cpp.hpp"
#include "platforms/esp/32/drivers/uart_esp32.cpp.hpp"

// Subdirectory implementations (alphabetical order)
#include "platforms/esp/32/drivers/gpio_isr_rx/_build.hpp"
#include "platforms/esp/32/drivers/i2s/_build.hpp"
#include "platforms/esp/32/drivers/lcd_cam/_build.hpp"
#include "platforms/esp/32/drivers/parlio/_build.hpp"
#include "platforms/esp/32/drivers/rmt/_build.hpp"
#include "platforms/esp/32/drivers/rmt_rx/_build.hpp"
#include "platforms/esp/32/drivers/spi/_build.hpp"
#include "platforms/esp/32/drivers/spi_ws2812/_build.hpp"
#include "platforms/esp/32/drivers/uart/_build.hpp"
