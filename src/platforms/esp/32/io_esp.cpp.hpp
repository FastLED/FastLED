
// TODO: Enable ESP-IDF UART driver for printing. Currently disabled.

// ok no namespace fl

#define FL_ESP_USE_IDF_SERIAL 0

#if FL_ESP_USE_IDF_SERIAL
// src\platforms\esp\32\io_esp_idf.hpp
#include "platforms/esp/32/detail/io_uart.hpp"
#else
#include "platforms/arduino/io_arduino.hpp"
#endif
