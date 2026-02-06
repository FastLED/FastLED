
// TODO: Enable ESP-IDF UART driver for printing. Currently disabled.

// ok no namespace fl

#define FL_ESP_USE_IDF_SERIAL 0

#if FL_ESP_USE_IDF_SERIAL
#include "platforms/esp/32/uart_esp32_idf.hpp"
#else
#include "platforms/arduino/io_arduino.hpp"
#endif
