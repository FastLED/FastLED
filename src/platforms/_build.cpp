/// @file _build.cpp
/// @brief Master unity build for platforms/ library
/// Compiles entire platforms/ namespace into single object file
/// This is the ONLY .cpp file in src/platforms/** that should be compiled

// Root directory implementations
#include "platforms/_build.hpp"

// Subdirectory implementations (alphabetical order)
#include "platforms/adafruit/_build.hpp"
#include "platforms/arduino/_build.hpp"
#include "platforms/arm/d21/_build.hpp"
#include "platforms/arm/d51/_build.hpp"
#include "platforms/arm/mgm240/_build.hpp"
#include "platforms/arm/nrf52/_build.hpp"
#include "platforms/arm/rp/_build.hpp"
#include "platforms/arm/rp/rp2040/_build.hpp"
#include "platforms/arm/rp/rpcommon/_build.hpp"
#include "platforms/arm/stm32/_build.hpp"
// Note: platforms/arm/stm32/drivers/_build.hpp removed - spi_hw_manager_stm32.cpp.hpp
// is included via platforms/arm/init_spi_hw.h dispatch header
#include "platforms/arm/teensy/_build.hpp"
#include "platforms/arm/teensy/teensy4_common/_build.hpp"
#include "platforms/avr/_build.hpp"
#include "platforms/avr/attiny/_build.hpp"
#include "platforms/esp/_build.hpp"
#include "platforms/esp/32/_build.hpp"
#include "platforms/esp/32/audio/_build.hpp"
#include "platforms/esp/32/core/_build.hpp"
#include "platforms/esp/32/drivers/_build.hpp"
#include "platforms/esp/32/drivers/gpio_isr_rx/_build.hpp"
#include "platforms/esp/32/drivers/i2s/_build.hpp"
#include "platforms/esp/32/drivers/lcd_cam/_build.hpp"
#include "platforms/esp/32/drivers/parlio/_build.hpp"
#include "platforms/esp/32/drivers/rmt/rmt_4/_build.hpp"
#include "platforms/esp/32/drivers/rmt/rmt_5/_build.hpp"
#include "platforms/esp/32/drivers/rmt_rx/_build.hpp"
#include "platforms/esp/32/drivers/spi/_build.hpp"
#include "platforms/esp/32/drivers/spi_ws2812/_build.hpp"
#include "platforms/esp/32/drivers/uart/_build.hpp"
#include "platforms/esp/32/interrupts/_build.hpp"
#include "platforms/esp/32/ota/_build.hpp"
#include "platforms/esp/8266/_build.hpp"
#include "platforms/posix/_build.hpp"
#include "platforms/shared/_build.hpp"
#include "platforms/shared/active_strip_data/_build.hpp"
#include "platforms/shared/mock/esp/32/drivers/_build.hpp"
#include "platforms/shared/spi_bitbang/_build.hpp"
#include "platforms/shared/ui/json/_build.hpp"
#include "platforms/stub/_build.hpp"
#include "platforms/wasm/_build.hpp"
#include "platforms/win/_build.hpp"
