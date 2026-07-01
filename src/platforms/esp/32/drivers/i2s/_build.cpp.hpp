// IWYU pragma: private

/// @file _build.hpp
/// @brief Unity build header for platforms\esp\32\drivers\i2s/ directory
///
/// I2S (LCD_CAM clockless on ESP32-S3) is never a platform default. Post-#2428
/// the driver impl is always compiled here so that
/// `fl::enableDriver<fl::Bus::FLEX_IO, 0>()` / `FastLED.enableAllDrivers()` /
/// `FastLED.setExclusiveDriver<fl::Bus::FLEX_IO, 0>()` have the symbols they
/// need to link. Default builds don't ODR-use any symbol from this driver, so
/// `--gc-sections` strips the whole TU.

#include "platforms/esp/32/drivers/i2s/channel_driver_i2s.cpp.hpp"
#include "platforms/esp/32/drivers/i2s/channel_engine_i2s_esp32dev.cpp.hpp"
#include "platforms/esp/32/drivers/i2s/clockless_i2s_esp32.cpp.hpp"

#include "platforms/esp/32/drivers/i2s/i2s_esp32dev.cpp.hpp"
#include "platforms/esp/32/drivers/i2s/i2s_lcd_cam_peripheral_esp.cpp.hpp"
#include "platforms/esp/32/drivers/i2s/i2s_lcd_cam_peripheral_mock.cpp.hpp"
#include "platforms/esp/32/drivers/i2s/i2s_peripheral_esp32dev_mock.cpp.hpp"
// The real-hardware peripheral (`i2s_peripheral_esp32dev_esp.cpp.hpp`)
// exists as a compilable TU but is intentionally NOT included in the
// classic-ESP32 build yet — its `#include "driver/i2s.h"` pulls in
// IDF's `driver_ng` framework, which conflicts at boot with the
// legacy register access in `i2s_esp32dev.cpp.hpp` (Yves Bazin's
// long-standing driver) causing an ADC driver_ng-vs-legacy abort at
// setup(). Wiring the real-hw impl requires deleting the Yves TU
// first — Stage 3 scope (see the follow-up issue).

#include "platforms/esp/32/drivers/i2s/wave8_encoder_i2s.cpp.hpp"

// Legacy concrete I2S driver implementation.
#include "platforms/esp/32/drivers/i2s/bus_traits.h"
