#ifndef _FASTLED_ESP32_H
#define _FASTLED_ESP32_H

#include "fastpin_esp32.h"
#include "third_party/espressif/led_strip/src/enabled.h"

#ifdef FASTLED_ALL_PINS_HARDWARE_SPI
#include "fastspi_esp32.h"
#endif


#ifdef FASTLED_ESP32_I2S

#ifndef FASTLED_INTERNAL
// Be careful with including defs for internal code.
#include "clockless_i2s_esp32.h"
#endif

#else

#if FASTLED_ESP32_HAS_RMT && !defined(FASTLED_ESP32_USE_CLOCKLESS_SPI)
#include "clockless_rmt_esp32.h"
#elif FASTLED_ESP32_HAS_CLOCKLESS_SPI
#include "clockless_spi_esp32.h"
// Note that this driver only works with the WS2811x family of LEDs.
// Other types of WS281x or leds with significantly different timings
// will not work with this driver but will fail silently to render pixels.
#define ClocklessController ClocklessSpiWs2812Controller
#define FASTLED_HAS_CLOCKLESS 1
#else
#warning "No clockless drivers defined for ESP32 chip. You won't be able to drive WS2812 and other clockless chipsets".
#endif

#endif


#endif // _FASTLED_ESP32_H