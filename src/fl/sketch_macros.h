#pragma once

#include "ftl/ostream.h"
#include "ftl/time.h"
#include "fl/delay.h"

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

#ifndef SKETCH_STRINGIFY
#define SKETCH_STRINGIFY_HELPER(x) #x
#define SKETCH_STRINGIFY(x) SKETCH_STRINGIFY_HELPER(x)
#endif

// SKETCH_HALT(msg) - Halts sketch execution and prints diagnostic message
// Prints immediately and then once every 5 seconds: "HALT at file(line): msg"
#define SKETCH_HALT(msg) \
    do { \
        static fl::u32 last_print_time = 0; \
        fl::u32 current_time = fl::time(); \
        if (last_print_time == 0 || (current_time - last_print_time) >= 5000) { \
            fl::cout << "ERROR: HALT at " << __FILE__ << "(" << __LINE__ << "): " << msg << "\n"; \
            last_print_time = current_time; \
        } \
        fl::delayMillis(100); \
    } while (1)
