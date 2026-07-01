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

// Classic-ESP32 I2S parallel-out clockless driver.
// The `FASTLED_ESP32_I2S` compile-time selector was removed per
// FastLED#3516 — the driver header is now included whenever the platform
// supports it (via `FASTLED_ESP32_HAS_I2S` feature flag), so
// `--gc-sections` can elide it when unused. Users select the driver at
// runtime via `FastLED.enableDriver<fl::Bus::FLEX_IO, 0>()` (or the
// modern `FastLED.add<Channel::create<>>()` surface). Legacy
// `addLeds<>` continues to route to the RMT / PARLIO / SPI default.
// IDF 6+ path routes through `i2s_periph_compat.h`'s LL API shim.
#if FASTLED_ESP32_HAS_I2S && !defined(FASTLED_INTERNAL)
#include "platforms/esp/32/drivers/i2s/clockless_i2s_esp32.h"
#endif

// Bulk controller implementations have been replaced by the Channel API
// See src/fl/channels/README.md for multi-strip support



#endif // _FASTLED_ESP32_H
