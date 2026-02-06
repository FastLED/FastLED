
// ok no namespace fl

// FL_ESP_USE_IDF_SERIAL controls whether to use ESP-IDF UART driver or Arduino Serial
#ifndef FL_ESP_USE_IDF_SERIAL
#define FL_ESP_USE_IDF_SERIAL 0
#endif

#if FL_ESP_USE_IDF_SERIAL
// Use ESP-IDF UART driver via fl::platforms wrapper
// Note: uart_esp32_idf.hpp is included via drivers/_build.hpp
#include "platforms/esp/32/io_esp_idf.hpp"
#else
// Use Arduino Serial interface
#include "platforms/arduino/io_arduino.hpp"
#endif
