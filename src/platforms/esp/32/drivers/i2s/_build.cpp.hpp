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

// FastLED#3526 Phase 2e — Yves `clockless_i2s_esp32.{h,cpp.hpp}` and
// `i2s_esp32dev.{h,cpp.hpp}` DELETED. Modern I2S engine is now the
// only path via `Bus::FLEX_IO, 0`.
#include "platforms/esp/32/drivers/i2s/i2s_lcd_cam_peripheral_esp.cpp.hpp"
#include "platforms/esp/32/drivers/i2s/i2s_lcd_cam_peripheral_mock.cpp.hpp"
// FastLED#3512 Phase 5: `i2s_peripheral_esp32dev_esp.cpp.hpp` is
// re-included here. Prior comments claimed the file's `driver/gpio.h`
// include triggered an `ADC: CONFLICT!` boot loop when linked
// alongside the restored classic Yves driver — that turned out to be
// specifically about `driver/i2s.h` from the earlier Stage 2 (#3476)
// impl, not `driver/gpio.h`. The current Stage 4 impl doesn't include
// `driver/i2s.h` at all (only very-low-level headers per the file's
// own doc comment), so the conflict rationale no longer applies. The
// stub `transmit()` currently does no DMA (see FastLED#3512 Phase 2
// for that work), so re-including adds a small TU that gc-sections
// elides completely when nothing calls into `I2sPeripheralEsp32DevEsp`.
#include "platforms/esp/32/drivers/i2s/i2s_peripheral_esp32dev_esp.cpp.hpp"
#include "platforms/esp/32/drivers/i2s/i2s_peripheral_esp32dev_mock.cpp.hpp"
#include "platforms/esp/32/drivers/i2s/i2s_port_claim.cpp.hpp"  // FastLED#3576 Phase 1 — cross-driver I2S0/I2S1 ownership

#include "platforms/esp/32/drivers/i2s/wave8_encoder_i2s.cpp.hpp"
#include "platforms/esp/32/drivers/i2s/wave8_encoder_i2s1.cpp.hpp"  // FastLED#3526 Phase 2a — general clockless encoder for I2S1

// Legacy concrete I2S driver implementation.
#include "platforms/esp/32/drivers/i2s/bus_traits.h"
