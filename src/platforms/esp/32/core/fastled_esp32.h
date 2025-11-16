// ok no namespace fl
#ifndef _FASTLED_ESP32_H
#define _FASTLED_ESP32_H

#include "fastpin_esp32.h"
#include "platforms/esp/32/feature_flags/enabled.h"

#ifdef FASTLED_ALL_PINS_HARDWARE_SPI
#include "fastspi_esp32.h"
#endif


// Include ALL available clockless drivers for ESP32
// Each driver defines its own type (ClocklessRMT, ClocklessSPI, ClocklessI2S)
// The platform-default ClocklessController alias is defined in chipsets.h

#if FASTLED_ESP32_HAS_RMT
#include "platforms/esp/32/drivers/rmt/clockless_rmt_esp32.h"
#endif

#if FASTLED_ESP32_HAS_CLOCKLESS_SPI
#include "platforms/esp/32/drivers/spi_ws2812/clockless_spi_esp32.h"
#endif

#ifdef FASTLED_ESP32_I2S
#ifndef FASTLED_INTERNAL
#include "platforms/esp/32/drivers/i2s/clockless_i2s_esp32.h"
#endif
#endif

// Bulk controller implementations have been replaced by the Channel API
// See src/fl/channels/README.md for multi-strip support



#endif // _FASTLED_ESP32_H