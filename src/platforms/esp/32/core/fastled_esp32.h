// IWYU pragma: private

// ok no namespace fl
#ifndef _FASTLED_ESP32_H
#define _FASTLED_ESP32_H

// IWYU pragma: begin_keep
#include "platforms/esp/32/core/fastpin_esp32.h"
#include "platforms/esp/32/feature_flags/enabled.h"
// IWYU pragma: end_keep

// ESP32 always uses hardware SPI via GPIO matrix - no conditional needed
// IWYU pragma: begin_keep
#include "platforms/esp/32/core/fastspi_esp32.h"
// IWYU pragma: end_keep


// Include ALL available clockless drivers for ESP32
// Each driver defines its own type (ClocklessRMT, ClocklessSPI, ClocklessI2S)
// The platform-default ClocklessController alias is defined in chipsets.h

#if FASTLED_ESP32_HAS_RMT
#include "platforms/esp/32/drivers/rmt/clockless_rmt_esp32.h"
#endif

#if FASTLED_ESP32_HAS_PARLIO
#include "platforms/esp/32/drivers/parlio/clockless_parlio_esp32.h"
#endif

#if FASTLED_ESP32_HAS_CLOCKLESS_SPI
#include "platforms/esp/32/drivers/spi/idf5_clockless_spi_esp32.h"
#endif

// The former Yves I2S parallel-out clockless driver
// (`clockless_i2s_esp32.h` + `i2s_esp32dev.{h,cpp.hpp}`) was deleted at
// FastLED#3526 Phase 2e. The modern I2S engine lives on
// `Bus::FLEX_IO, 0` via the channel manager; users select it at runtime
// via `FastLED.enableDriver<fl::Bus::FLEX_IO, 0>()` or
// `FastLED.setExclusiveDriver<fl::Bus::FLEX_IO, 0>()`.

// Bulk controller implementations have been replaced by the Channel API
// See src/fl/channels/README.md for multi-strip support



#endif // _FASTLED_ESP32_H
