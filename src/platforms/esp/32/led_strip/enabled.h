
#pragma once

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
