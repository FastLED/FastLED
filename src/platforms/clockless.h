#pragma once

/// @file clockless.h
/// Platform-specific clockless controller dispatch header.
/// This header includes the appropriate clockless controller implementation
/// based on the target platform.

// Platform-specific clockless controller includes
#if defined(__EMSCRIPTEN__)
  #include "wasm/clockless.h"
#elif defined(FASTLED_STUB_IMPL) && !defined(__EMSCRIPTEN__)
  #include "stub/clockless_stub_generic.h"
#elif defined(ESP32) || defined(ARDUINO_ARCH_ESP32) || defined(ESP8266) || defined(ARDUINO_ARCH_ESP8266)
  #include "esp/clockless.h"
#elif defined(FASTLED_TEENSY4)
  #include "arm/teensy/teensy4_common/clockless_arm_mxrt1062.h"
#elif defined(__AVR__)
  #include "lib8tion/attiny_detect.h"
  #if defined(LIB8_ATTINY)
    // ATtiny platforms use optimized assembly implementation from FastLED 3.10.3
    #include "attiny/clockless_blocking.h"
  #else
    // Other AVR platforms (Uno, Mega, etc.) use the standard clockless controller
    #include "avr/clockless_trinket.h"
  #endif
#endif

// Generic ClocklessBlocking controller is available at platforms/shared/clockless_blocking.h
// Platforms should explicitly include and use it as a fallback if they don't have
// a hardware-accelerated implementation

// Template alias to ClocklessController (platform-specific or generic blocking)
// This must come AFTER all clockless drivers are included
#include "fl/clockless_controller_impl.h"
