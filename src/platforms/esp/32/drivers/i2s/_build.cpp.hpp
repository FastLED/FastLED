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
// Stage 5 (#3474): the proven register+DMA+ISR machinery from the
// classic-ESP32 I2S driver is restored above under
// `i2s_esp32dev.{h,cpp.hpp}` + `clockless_i2s_esp32.{h,cpp.hpp}` —
// these are the ~700 LoC that generate real WS2812 parallel-out
// waveforms on I2S1. Users on classic ESP32 can hit that path via
// the legacy `addLeds<WS2812, PIN, GRB>()` template. The modern
// `ChannelEngineI2sEsp32Dev` + mock (still shipped above) exposes
// the same peripheral behind a mock-testable IChannelDriver
// interface for future code that wants the modern channel-manager
// path. `i2s_peripheral_esp32dev_esp.cpp.hpp` (Stage 4 real-hw impl)
// is intentionally NOT included here — its `driver/gpio.h` include
// collides with the restored code at link time as an
// `ADC: CONFLICT!` boot loop. A future PR that reworks the modern
// peripheral to share I2S1/periph_module state (rather than
// duplicating it) can drop the modern impl into the unity build
// cleanly.

#include "platforms/esp/32/drivers/i2s/wave8_encoder_i2s.cpp.hpp"

// Legacy concrete I2S driver implementation.
#include "platforms/esp/32/drivers/i2s/bus_traits.h"
