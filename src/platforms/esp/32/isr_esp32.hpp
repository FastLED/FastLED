/*
  FastLED — ESP32 ISR Implementation (Version Trampoline)
  -------------------------------------------------------
  ESP32-specific implementation of the cross-platform ISR API.
  Supports ESP32, ESP32-S2, ESP32-S3 (Xtensa) and ESP32-C3, ESP32-C6 (RISC-V).

  This header routes to the appropriate implementation based on ESP-IDF version:
  - ESP-IDF 5.0+: Uses gptimer API (isr_esp32_idf5.hpp)
  - ESP-IDF 4.x:  Uses legacy timer API with timer_isr_callback_* (isr_esp32_idf4.hpp)
  - ESP-IDF 3.x:  Uses legacy timer API with timer_isr_register (isr_esp32_idf3.hpp)

  License: MIT (FastLED)
*/

#pragma once

// ok no namespace fl - dispatch header includes platform-specific implementations

#if defined(ESP32)

#include "fl/compiler_control.h"

// Use FastLED's ESP-IDF version detection header (handles IDF 3.3+)
#include "platforms/esp/esp_version.h"

// Route to appropriate implementation based on ESP-IDF version
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
    // ESP-IDF 5.0+: Use new gptimer API
    #include "platforms/esp/32/isr_esp32_idf5.hpp"
#elif ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(4, 0, 0)
    // ESP-IDF 4.x: Use legacy timer API with callback functions
    #include "platforms/esp/32/isr_esp32_idf4.hpp"
#else
    // ESP-IDF 3.x: Use legacy timer API with timer_isr_register
    #include "platforms/esp/32/isr_esp32_idf3.hpp"
#endif

#endif // ESP32
