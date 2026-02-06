
// TODO: Enable ESP-IDF UART driver for printing. Currently disabled.

// ok no namespace fl

// FL_ESP_USE_IDF_SERIAL is defined in io_esp.cpp.hpp
#ifndef FL_ESP_USE_IDF_SERIAL
#define FL_ESP_USE_IDF_SERIAL 0
#endif

#if FL_ESP_USE_IDF_SERIAL
#include "platforms/esp/32/drivers/uart_esp32_idf.hpp"
#else
#include "platforms/arduino/io_arduino.hpp"
#endif
