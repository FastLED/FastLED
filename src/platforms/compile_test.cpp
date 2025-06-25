#define FASTLED_INTERNAL  
#include "FastLED.h"

// In the future, one of these might fail, and the user will want
// to disable the compile tests so that they can continue on their
// way.
// If for some reason you are hitting this then use a BUILD define
// (not an include define) in your build flags section
// '-DFASTLED_USE_COMPILE_TESTS=0'
#ifndef FASTLED_USE_COMPILE_TESTS
#define FASTLED_USE_COMPILE_TESTS 1
#endif


#if FASTLED_USE_COMPILE_TESTS


#include "fl/compiler_control.h"

FL_DISABLE_WARNING_PUSH

FL_DISABLE_WARNING(unused-function)
FL_DISABLE_WARNING(unused-parameter)


// Include platform-specific compile test headers
#if defined(__AVR__)
#include "avr/compile_test.hpp"
#elif defined(ESP32) || defined(ESP8266)
#include "esp/compile_test.hpp"
#elif defined(FASTLED_ARM)
#include "arm/compile_test.hpp"
#elif defined(APOLLO3) || defined(ARDUINO_ARCH_APOLLO3)
#include "apollo3/compile_test.hpp"
#elif defined(FASTLED_STUB_IMPL)
#include "stub/compile_test.hpp"
#endif

namespace fl {
// This file contains only compile-time tests.
// The platform-specific test functions are called to trigger
// any compile-time errors if the platform is not configured correctly.
static void compile_tests() {
#if defined(__AVR__)
    avr_compile_tests();
#elif defined(ESP32)
    esp32_compile_tests();
#elif defined(ESP8266)
    esp8266_compile_tests();
#elif defined(FASTLED_ARM)
    arm_compile_tests();
#elif defined(APOLLO3) || defined(ARDUINO_ARCH_APOLLO3)
    apollo3_compile_tests();
#elif defined(FASTLED_STUB_IMPL)
    stub_compile_tests();
#else
    // No platform-specific tests available for this platform
    static_assert(false, "Unknown platform - no compile tests available");
#endif
}
}

FL_DISABLE_WARNING_POP

#endif  // FASTLED_USE_COMPILE_TESTS
