// IWYU pragma: private

/// @file _build.hpp
/// @brief Unity build header for platforms\esp\32\drivers/ directory
/// Includes all implementation files in alphabetical order
///
/// Post-#2428 (binary-size fix for #2420 / #2421):
///
/// Each LED-output driver subdirectory's `_build.cpp.hpp` only compiles its
/// implementation on platforms where the driver is the platform default
/// (PARLIO on P4/C6/H2/C5, RMT elsewhere, LCD_SPI on S3, I2S_SPI on dev).
/// All other drivers are pure opt-in via `fl::enableDrivers<fl::Bus::X>()`
/// or `FastLED.enableAllDrivers()`, which include the driver's `bus_traits.h`
/// at the call site and ODR-link the TU. The auxiliary subdirs (`ble`,
/// `gpio_isr_rx`, `rmt_rx`) are not channel drivers and stay unconditional.
///
/// Unconditional aggregator structure (one include per subdir, alphabetical)
/// is preserved to satisfy the unity-build linter.

// Root directory implementations (alphabetical order)

// begin current directory includes
#include "platforms/esp/32/drivers/channel_manager_esp32.cpp.hpp"
#include "platforms/esp/32/drivers/cled.cpp.hpp"
#include "platforms/esp/32/drivers/spi_hw_manager_esp32.cpp.hpp"
#include "platforms/esp/32/drivers/uart_esp32_idf.hpp"
#include "platforms/esp/32/drivers/usb_serial_jtag_esp32_idf.hpp"

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
