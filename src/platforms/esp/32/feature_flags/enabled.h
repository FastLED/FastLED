// ok no namespace fl
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

// PARLIO driver availability
// PARLIO is available on ESP32-P4, ESP32-C6, ESP32-H2, and ESP32-C5 with ESP-IDF 5.0+
// Note: ESP32-S3 does NOT have PARLIO hardware (it has LCD peripheral instead)
#if !defined(FASTLED_ESP32_HAS_PARLIO)
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
#if defined(CONFIG_IDF_TARGET_ESP32P4) || defined(CONFIG_IDF_TARGET_ESP32C6) || \
    defined(CONFIG_IDF_TARGET_ESP32H2) || defined(CONFIG_IDF_TARGET_ESP32C5)
#define FASTLED_ESP32_HAS_PARLIO 1
#else
#define FASTLED_ESP32_HAS_PARLIO 0
#endif
#else
#define FASTLED_ESP32_HAS_PARLIO 0  // Requires ESP-IDF 5.0+
#endif
#endif

#endif  // ESP32 || ARDUINO_ARCH_ESP32
