
#pragma once

#ifndef FASTLED_ESP32_COMPONENT_LED_STRIP_FORCE_IDF4
#define FASTLED_ESP32_COMPONENT_LED_STRIP_FORCE_IDF4 0
#endif

#ifndef ESP32
// No led strip component when not in ESP32 mode.
 #define FASTLED_RMT51 0
#else
 #if !defined(FASTLED_RMT51)
  #include "esp_idf_version.h"
  #if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
   #define FASTLED_RMT51 1
  #else
   #define FASTLED_RMT51 0
  #endif  // ESP_IDF_VERSION
 #endif  // FASTLED_RMT51
#endif  // ESP32
