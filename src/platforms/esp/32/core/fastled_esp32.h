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

#ifdef FASTLED_ESP32_I2S
#include "platforms/esp/esp_version.h"
#if ESP_IDF_VERSION_6_OR_HIGHER
// I2S parallel driver is not yet ported to ESP-IDF 6.0+.
// PERIPH_I2S1_MODULE was removed; the driver needs LL API migration.
// Falling through to RMT/SPI/blocking driver instead.
#else
#ifndef FASTLED_INTERNAL
#include "platforms/esp/32/drivers/i2s/clockless_i2s_esp32.h"
#endif
#endif // ESP_IDF_VERSION_6_OR_HIGHER
#endif // FASTLED_ESP32_I2S

// Bulk controller implementations have been replaced by the Channel API
// See src/fl/channels/README.md for multi-strip support



#endif // _FASTLED_ESP32_H
