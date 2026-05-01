// IWYU pragma: private

/// @file _build.hpp
/// @brief Unity build header for platforms\esp\32\drivers/ directory
/// Includes all implementation files in alphabetical order

#include "fl/stl/compiler_control.h"
FL_NO_UNWIND_BEGIN

// Root directory implementations (alphabetical order)

// begin current directory includes
#include "platforms/esp/32/drivers/channel_manager_esp32.cpp.hpp"
#include "platforms/esp/32/drivers/cled.cpp.hpp"
#include "platforms/esp/32/drivers/spi_hw_manager_esp32.cpp.hpp"

// begin sub directory includes
#include "platforms/esp/32/drivers/ble/_build.cpp.hpp"
#include "platforms/esp/32/drivers/gpio_isr_rx/_build.cpp.hpp"
#include "platforms/esp/32/drivers/i2s/_build.cpp.hpp"
#include "platforms/esp/32/drivers/i2s_spi/_build.cpp.hpp"
#include "platforms/esp/32/drivers/lcd_cam/_build.cpp.hpp"
#include "platforms/esp/32/drivers/lcd_spi/_build.cpp.hpp"
#include "platforms/esp/32/drivers/parlio/_build.cpp.hpp"
#include "platforms/esp/32/drivers/rmt/_build.cpp.hpp"
#include "platforms/esp/32/drivers/rmt_rx/_build.cpp.hpp"
#include "platforms/esp/32/drivers/spi/_build.cpp.hpp"
#include "platforms/esp/32/drivers/uart/_build.cpp.hpp"

FL_NO_UNWIND_END
