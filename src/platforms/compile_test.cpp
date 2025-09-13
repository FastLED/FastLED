#define FASTLED_INTERNAL  
#include "FastLED.h"
#include "fl/int.h"
#include "fl/stdint.h"
#include "fl/type_traits.h"

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

    static_assert(fl::is_same<u32, uint32_t>::value, "u32 must be the same type as uint32_t");
    static_assert(fl::is_same<u16, uint16_t>::value, "u16 must be the same type as uint16_t");
    static_assert(fl::is_same<u8, uint8_t>::value, "u8 must be the same type as uint8_t");
    static_assert(fl::is_same<i32, int32_t>::value, "i32 must be the same type as int32_t");
    static_assert(fl::is_same<i16, int16_t>::value, "i16 must be the same type as int16_t");
    static_assert(fl::is_same<i8, int8_t>::value, "i8 must be the same type as int8_t");
    static_assert(fl::is_same<size, size_t>::value, "size must be the same type as size_t");
    static_assert(fl::is_same<uptr, uintptr_t>::value, "uptr must be the same type as uintptr_t");

    // Size assertions for FastLED integer types
    static_assert(sizeof(i8) == 1, "i8 must be exactly 1 byte");
    static_assert(sizeof(u8) == 1, "u8 must be exactly 1 byte");
    static_assert(sizeof(i16) == 2, "i16 must be exactly 2 bytes");
    static_assert(sizeof(u16) == 2, "u16 must be exactly 2 bytes");
    static_assert(sizeof(i32) == 4, "i32 must be exactly 4 bytes");
    static_assert(sizeof(u32) == 4, "u32 must be exactly 4 bytes");
    static_assert(sizeof(i64) == 8, "i64 must be exactly 8 bytes");
    static_assert(sizeof(u64) == 8, "u64 must be exactly 8 bytes");
    static_assert(sizeof(uptr) == sizeof(uintptr_t), "uptr must be exactly the same size as uintptr_t");
    static_assert(sizeof(size) == sizeof(size_t), "size must be exactly the same size as size_t");


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
