#pragma once


#if __has_include("soc/soc_caps.h")
  #include "soc/soc_caps.h"
#endif

#if defined(SOC_RMT_SUPPORTED) && SOC_RMT_SUPPORTED
#define FASTLED_ESP32_HAS_RMT 1
#else
#define FASTLED_ESP32_HAS_RMT 0
#endif

#ifndef FASTLED_ESP_HAS_CLOCKLESS_SPI
#if !__has_include("driver/rmt_types.h") && !CONFIG_IDF_TARGET_ESP32C2
#define FASTLED_ESP_HAS_CLOCKLESS_SPI 0
#else
#define FASTLED_ESP_HAS_CLOCKLESS_SPI 1
#endif
#endif

#ifndef ESP32
// No led strip component when not in ESP32 mode.
 #define FASTLED_RMT5 0
#else
 #if !defined(FASTLED_RMT5)
  #include "platforms/esp/esp_version.h"
  #if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
   #define FASTLED_RMT5 1
  #else
   #define FASTLED_RMT5 0
  #endif  // ESP_IDF_VERSION
 #endif  // FASTLED_RMT5
#endif  // ESP32
