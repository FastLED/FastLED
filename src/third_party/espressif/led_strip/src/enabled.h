#pragma once

#ifdef ESP32
#include "sdkconfig.h"
#include "platforms/esp/esp_version.h"

#if CONFIG_IDF_TARGET_ESP32C2
#define FASTLED_ESP32_HAS_CLOCKLESS_SPI 1
#define FASTLED_ESP32_HAS_RMT 0
#define FASTLED_ESP32_HAS_RMT5 0
#elif CONFIG_IDF_TARGET_ESP32C3
#define FASTLED_ESP32_HAS_CLOCKLESS_SPI 1
#define FASTLED_ESP32_HAS_RMT 1
#define FASTLED_ESP32_HAS_RMT5 1
#elif CONFIG_IDF_TARGET_ESP32C6
#define FASTLED_ESP32_HAS_CLOCKLESS_SPI 1
#define FASTLED_ESP32_HAS_RMT 1
#define FASTLED_ESP32_HAS_RMT5 1
#elif CONFIG_IDF_TARGET_ESP32S2
#define FASTLED_ESP32_HAS_CLOCKLESS_SPI 1
#define FASTLED_ESP32_HAS_RMT 1
#define FASTLED_ESP32_HAS_RMT5 1
#elif CONFIG_IDF_TARGET_ESP32S3
#define FASTLED_ESP32_HAS_CLOCKLESS_SPI 1
#define FASTLED_ESP32_HAS_RMT 1
#define FASTLED_ESP32_HAS_RMT5 1
#elif CONFIG_IDF_TARGET_ESP32H2
#define FASTLED_ESP32_HAS_CLOCKLESS_SPI 1
#define FASTLED_ESP32_HAS_RMT 1
#define FASTLED_ESP32_HAS_RMT5 1
#elif CONFIG_IDF_TARGET_ESP32P4
#define FASTLED_ESP32_HAS_CLOCKLESS_SPI 1
#define FASTLED_ESP32_HAS_RMT 1
#define FASTLED_ESP32_HAS_RMT5 1
#elif CONFIG_IDF_TARGET_ESP8266
#define FASTLED_ESP32_HAS_CLOCKLESS_SPI 0
#define FASTLED_ESP32_HAS_RMT 0
#define FASTLED_ESP32_HAS_RMT5 0
#elif CONFIG_IDF_TARGET_ESP32 || defined(ARDUINO_ESP32_DEV)
#define FASTLED_ESP32_HAS_CLOCKLESS_SPI 1
#define FASTLED_ESP32_HAS_RMT 1
#define FASTLED_ESP32_HAS_RMT5 1
#else
#warning "Unknown board, assuming support for clockless RMT5 and SPI chipsets. Please file an bug report with FastLED and tell them about your board type."
#endif

// Ensure the feature macros default to *enabled* even if earlier code set them
// to 0 or left them undefined.  Any board that truly lacks RMT-5 support
// should set the flag to 0 *after* this header is included.
#if !defined(FASTLED_ESP32_HAS_RMT5) || (FASTLED_ESP32_HAS_RMT5 == 0)
#undef FASTLED_ESP32_HAS_RMT5
#define FASTLED_ESP32_HAS_RMT5 1
#endif

#if !defined(FASTLED_ESP32_HAS_CLOCKLESS_SPI) || (FASTLED_ESP32_HAS_CLOCKLESS_SPI == 0)
#undef FASTLED_ESP32_HAS_CLOCKLESS_SPI
#define FASTLED_ESP32_HAS_CLOCKLESS_SPI 1
#endif

// Legacy macro FASTLED_RMT5 should mirror FASTLED_ESP32_HAS_RMT5.
#undef FASTLED_RMT5
#define FASTLED_RMT5 FASTLED_ESP32_HAS_RMT5

#endif  // ESP32
