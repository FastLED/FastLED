#pragma once

// IWYU pragma: private

#include "platforms/esp/is_esp.h"  // IWYU pragma: keep

#ifdef FL_IS_ESP32

// ok no namespace fl

#include "fl/has_include.h"

// FL_ESP_USE_IDF_SERIAL controls whether to use ESP-IDF UART driver or Arduino Serial
// Auto-detect: Use IDF if Arduino is not available, otherwise default to Arduino
#ifndef FL_ESP_USE_IDF_SERIAL
    #if !FL_HAS_INCLUDE(<Arduino.h>)
        #define FL_ESP_USE_IDF_SERIAL 1  // Arduino not available - use ESP-IDF
    #else
        #define FL_ESP_USE_IDF_SERIAL 0  // Arduino available - use Arduino Serial by default
    #endif
#endif

#if FL_ESP_USE_IDF_SERIAL
// Use ESP-IDF UART driver via fl::platforms wrapper
// Note: uart_esp32_idf.hpp is included via drivers/_build.hpp
#include "platforms/esp/32/io_esp_idf.hpp"
#else
// Use Arduino Serial interface
#include "platforms/arduino/io_arduino.hpp"
#endif

#endif // FL_IS_ESP32
