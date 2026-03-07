// ok no namespace fl
#pragma once

// IWYU pragma: private

#include "platforms/esp/8266/fastpin_esp8266.h"

#ifdef FASTLED_ALL_PINS_HARDWARE_SPI
#include "platforms/esp/8266/fastspi_esp8266.h"
#endif

#include "platforms/esp/8266/clockless_esp8266.h"
#include "platforms/esp/8266/clockless_block_esp8266.h"

#ifdef FASTLED_ESP8266_UART
#include "platforms/esp/8266/fastled_esp8266_uart.h"
#endif
