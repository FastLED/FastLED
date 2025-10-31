// ok no namespace fl
#ifndef _FASTLED_ESP32_H
#define _FASTLED_ESP32_H

#include "fastpin_esp32.h"
#include "third_party/espressif/led_strip/src/enabled.h"

#ifdef FASTLED_ALL_PINS_HARDWARE_SPI
#include "fastspi_esp32.h"
#endif


// Include ALL available clockless drivers for ESP32
// Each driver defines its own type (ClocklessRMT, ClocklessSPI, ClocklessI2S)
// The platform-default ClocklessController alias is defined in chipsets.h

#if FASTLED_ESP32_HAS_RMT
#include "clockless_rmt_esp32.h"
#endif

#if FASTLED_ESP32_HAS_CLOCKLESS_SPI
#include "clockless_spi_esp32.h"
#endif

#ifdef FASTLED_ESP32_I2S
#ifndef FASTLED_INTERNAL
#include "clockless_i2s_esp32.h"
#endif
#endif


#endif // _FASTLED_ESP32_H