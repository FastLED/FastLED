#pragma once


#include "soc/soc_caps.h"

#define FASTLED_ESP32_HAS_RMT (SOC_RMT_TX_CANDIDATES_PER_GROUP > 0)


#ifndef ESP32
// No led strip component when not in ESP32 mode.
 #define FASTLED_RMT5 0
#else
 #if !defined(FASTLED_RMT5)
  #include "platforms/esp/esp_version.h"
  #if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
   #define FASTLED_IDF5 1
  #else
   #define FASTLED_IDF5 0
  #endif  // ESP_IDF_VERSION
 #endif  // FASTLED_RMT5
#endif  // ESP32

#ifndef FASTLED_RMT5
#define FASTLED_RMT5 (FASTLED_IDF5 && FASTLED_ESP32_HAS_RMT)
#endif


#ifndef FASTLED_ESP_HAS_CLOCKLESS_SPI
#if !FASTLED_IDF5
#define FASTLED_ESP_HAS_CLOCKLESS_SPI 0
#else
#define FASTLED_ESP_HAS_CLOCKLESS_SPI 1
#endif
#endif
