#pragma once

#include "fl/stl/ostream.h"
#include "fl/stl/time.h"
#include "fl/delay.h"

#include "platforms/is_platform.h"

#if defined(__AVR__) \
  || defined(__AVR_ATtiny85__) \
  || defined(__AVR_ATtiny88__) \
  || defined(__AVR_ATmega32U4__) \
  || defined(ARDUINO_attinyxy6) \
  || defined(ARDUINO_attinyxy4) \
  || defined(FL_IS_TEENSY_LC) \
  || defined(FL_IS_TEENSY_30) \
  || defined(FL_IS_TEENSY_31) \
  || defined(FL_IS_TEENSY_32) \
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

// SKETCH_HALT and SKETCH_HALT_OK macros have been removed.
// They caused watchdog timer resets on ESP32-C6 due to the infinite while(1) loop
// preventing loop() from returning.
//
// Replacement: Use a SketchHalt helper class (see examples/Validation/SketchHalt.h
// or examples/RX/SketchHalt.h for reference implementation).
//
// Pattern:
//   1. Create a local SketchHalt.h header in your sketch directory
//   2. Include it and create a global halt object
//   3. First line of loop() MUST be: if (halt.check()) return;
//   4. Call halt.error("message") or halt.finish("message") to halt
//
// This allows loop() to return normally, preventing watchdog timer resets.
