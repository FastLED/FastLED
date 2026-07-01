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

#include "platforms/esp/32/drivers/i2s/i2s_lcd_cam_peripheral_esp.cpp.hpp"
#include "platforms/esp/32/drivers/i2s/i2s_lcd_cam_peripheral_mock.cpp.hpp"
#include "platforms/esp/32/drivers/i2s/i2s_peripheral_esp32dev_mock.cpp.hpp"
// The real-hardware peripheral (`i2s_peripheral_esp32dev_esp.cpp.hpp`)
// header and impl exist in the tree but are NOT included in the
// classic-ESP32 unity build yet. Its `#include "driver/i2s.h"` pulls
// in IDF's `driver_ng` framework, which conflicts at IDF init time
// with the legacy `driver/adc.h` path pulled in by
// `platforms/esp/32/pin_esp32_native_impl.hpp` — the resulting
// runtime abort is `ADC: CONFLICT! driver_ng is not allowed to be
// used with the legacy driver`. Wiring the real-hw impl requires
// migrating that ADC path to `esp_adc/adc_oneshot.h` first (or
// dropping the ADC include when only pinMode() is reached). Yves
// Bazin's third-party driver (`i2s_esp32dev.{h,cpp.hpp}` +
// `clockless_i2s_esp32.{h,cpp.hpp}`) is *deleted* in this PR —
// that removes 800+ LoC of legacy register access and clears the
// path forward.

#include "platforms/esp/32/drivers/i2s/wave8_encoder_i2s.cpp.hpp"

// Legacy concrete I2S driver implementation.
#include "platforms/esp/32/drivers/i2s/bus_traits.h"
