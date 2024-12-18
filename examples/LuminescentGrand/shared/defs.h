#pragma once


#if defined(__AVR__) || defined(ARDUINO_ARCH_AVR) || defined(ARDUINO_ARCH_ESP8266) || defined(ARDUINO_ARCH_RP2040) || defined(ARDUINO_ARCH_RP2350) || defined(ARDUINO_ARCH_RP2040)
#define ENABLE_SKETCH 0
#else
#define ENABLE_SKETCH 1
#endif