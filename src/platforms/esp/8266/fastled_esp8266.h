// ok no namespace fl
#pragma once

#include "fastpin_esp8266.h"

#ifdef FASTLED_ALL_PINS_HARDWARE_SPI
#include "fastspi_esp8266.h"
#endif

#include "clockless_esp8266.h"
#include "clockless_block_esp8266.h"

#ifdef FASTLED_ESP8266_UART
#include "fastled_esp8266_uart.h"
#endif
