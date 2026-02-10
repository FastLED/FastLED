#pragma once

#include "fl/stl/ostream.h"
#include "fl/stl/time.h"
#include "fl/delay.h"

#include "platforms/is_platform.h"

#if defined(FL_IS_AVR) \
  || defined(__AVR_ATtiny85__) \
  || defined(__AVR_ATtiny88__) \
  || defined(__AVR_ATmega32U4__) \
  || defined(ARDUINO_attinyxy6) \
  || defined(ARDUINO_attinyxy4) \
  || defined(FL_IS_TEENSY_LC) \
  || defined(FL_IS_TEENSY_30) \
  || defined(FL_IS_TEENSY_31) \
  || defined(FL_IS_TEENSY_32) \
  || defined(FL_IS_TEENSY_30) \
  || defined(FL_IS_TEENSY_3X) \
  || defined(FL_IS_STM32_F1) \
  || defined(FL_IS_ESP8266) \
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
// Replacement: Use a static flag to run tests once:
//
// Pattern:
//   static bool tests_run = false;
//
//   void loop() {
//       if (tests_run) {
//           delay(1000);
//           return;
//       }
//       tests_run = true;
//
//       // ... your test code here ...
//       FL_PRINT("Tests complete");
//   }
//
// This allows loop() to return normally, preventing watchdog timer resets.
