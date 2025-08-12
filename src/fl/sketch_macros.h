#pragma once

#if defined(__AVR__) \
  || defined(__AVR_ATtiny85__) \
  || defined(__AVR_ATtiny88__) \
  || defined(__AVR_ATmega32U4__) \
  || defined(ARDUINO_attinyxy6) \
  || defined(ARDUINO_attinyxy4) \
  || defined(ARDUINO_TEENSYLC) \
  || defined(ARDUINO_TEENSY30) \
  || defined(__MK20DX128__) \
  || defined(__MK20DX256__) \
  || defined(STM32F1) \
  || defined(ESP8266) \
  || defined(ARDUINO_ARCH_RENESAS_UNO) \
  || defined(ARDUINO_BLUEPILL_F103C8)
#define SKETCH_HAS_LOTS_OF_MEMORY 0
#else
#define SKETCH_HAS_LOTS_OF_MEMORY 1
#endif

#ifndef FASTLED_STRINGIFY
#define FASTLED_STRINGIFY_HELPER(x) #x
#define FASTLED_STRINGIFY(x) FASTLED_STRINGIFY_HELPER(x)
#endif
