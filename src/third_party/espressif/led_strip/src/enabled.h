#pragma once

#if defined(ESP32) || defined(ARDUINO_ARCH_ESP32)
#include "sdkconfig.h"
#include "platforms/esp/esp_version.h"

#if ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(5, 0, 0) || defined(CONFIG_IDF_TARGET_ESP8266)
#define FASTLED_ESP32_HAS_CLOCKLESS_SPI 0
#else
#define FASTLED_ESP32_HAS_CLOCKLESS_SPI 1
#endif


// Note that FASTLED_RMT5 is a legacy name,
// so we keep it because "RMT" is specific to ESP32
#if defined(FASTLED_RMT5)
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
#define FASTLED_RMT5 1
#else
#define FASTLED_RMT5 0  // Legacy driver
#endif
#endif

#endif  // ESP32 || ARDUINO_ARCH_ESP32
