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
#include "../drivers/rmt/clockless_rmt_esp32.h"
#endif

#if FASTLED_ESP32_HAS_CLOCKLESS_SPI
#include "../drivers/spi_ws2812/clockless_spi_esp32.h"
#endif

#ifdef FASTLED_ESP32_I2S
#ifndef FASTLED_INTERNAL
#include "../drivers/i2s/clockless_i2s_esp32.h"
#endif
#endif

// Include bulk controller implementations
// These provide multi-strip support using hardware peripherals

#if FASTLED_RMT5
#include "../drivers/rmt/bulk_rmt.h"
#endif

#if defined(CONFIG_IDF_TARGET_ESP32S3) || defined(CONFIG_IDF_TARGET_ESP32P4)
#include "../drivers/lcd_cam/bulk_lcd_i80.h"
#endif

#if defined(CONFIG_IDF_TARGET_ESP32P4)
#include "../drivers/parlio/bulk_parlio.h"
#endif

#if defined(CONFIG_IDF_TARGET_ESP32)
// ESP32 (original): I2S-based bulk controller
#include "../drivers/i2s/bulk_i2s.h"
#endif
// ESP32-S3: Use LCD_CAM-based bulk controller (included above at line 38-40)


#endif // _FASTLED_ESP32_H