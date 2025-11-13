#pragma once

#if defined(ESP32) || defined(ARDUINO_ARCH_ESP32)
#include "sdkconfig.h"
#include "platforms/esp/esp_version.h"

#if ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(5, 0, 0) || defined(CONFIG_IDF_TARGET_ESP8266)
#define FASTLED_ESP32_HAS_CLOCKLESS_SPI 0
#else
#define FASTLED_ESP32_HAS_CLOCKLESS_SPI 1
#endif

// RMT driver availability
// RMT is available on all ESP32 variants except ESP8266
#if !defined(CONFIG_IDF_TARGET_ESP8266) && !defined(CONFIG_IDF_TARGET_ESP32C2)
#define FASTLED_ESP32_HAS_RMT 1
#else
#define FASTLED_ESP32_HAS_RMT 0
#endif

// Note that FASTLED_RMT5 is a legacy name,
// so we keep it because "RMT" is specific to ESP32
// Auto-detect RMT5 based on ESP-IDF version if not explicitly defined
#if !defined(FASTLED_RMT5)
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0) && FASTLED_ESP32_HAS_RMT
#define FASTLED_RMT5 1
#else
#define FASTLED_RMT5 0  // Legacy driver or no RMT
#endif
#endif

#endif  // ESP32 || ARDUINO_ARCH_ESP32
