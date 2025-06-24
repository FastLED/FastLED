#pragma once

#if defined(__AVR__) ||defined(ARDUINO_TEENSYLC) || defined(ARDUINO_TEENSY30) || defined(__MK20DX256__) || defined(STM32F1) || defined(ESP8266) || defined(ARDUINO_ARCH_RENESAS_UNO)
#define SKETCH_HAS_LOTS_OF_MEMORY 0
#else
#define SKETCH_HAS_LOTS_OF_MEMORY 1
#endif
