#define FASTLED_INTERNAL  
#include "fl/fastled.h"
#include "fl/int.h"
#include "fl/stl/stdint.h"
#include "fl/stl/type_traits.h"

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
#include "platforms/avr/compile_test.hpp"
#elif defined(ESP32) || defined(ESP8266)
#include "platforms/esp/compile_test.hpp"
#elif defined(FASTLED_ARM)
#include "platforms/arm/compile_test.hpp"
#elif defined(APOLLO3) || defined(ARDUINO_ARCH_APOLLO3)
#include "platforms/apollo3/compile_test.hpp"
#elif defined(FASTLED_STUB_IMPL)
#include "platforms/stub/compile_test.hpp"
#endif

namespace fl {

// Test that StrStream and FakeStrStream can accept all fundamental integer types
// This ensures SFINAE collision prevention works across all platforms
FL_MAYBE_UNUSED static void test_strstream_integer_operators() {
    fl::StrStream ss;
    fl::FakeStrStream fss;

    // Test all fundamental integer types - ensures no ambiguous overloads
    char c = 'a'; ss << c; fss << c;
    signed char sc = 1; ss << sc; fss << sc;
    unsigned char uc = 2; ss << uc; fss << uc;
    short s = 3; ss << s; fss << s;
    unsigned short us = 4; ss << us; fss << us;
    int i = 5; ss << i; fss << i;
    unsigned int ui = 6; ss << ui; fss << ui;
    long l = 7; ss << l; fss << l;
    unsigned long ul = 8; ss << ul; fss << ul;
    long long ll = 9; ss << ll; fss << ll;
    unsigned long long ull = 10; ss << ull; fss << ull;

    // Test fl:: types (which are typedefs to fundamental types)
    i8 i8v = 11; ss << i8v; fss << i8v;
    u8 u8v = 12; ss << u8v; fss << u8v;
    i16 i16v = 13; ss << i16v; fss << i16v;
    u16 u16v = 14; ss << u16v; fss << u16v;
    i32 i32v = 15; ss << i32v; fss << i32v;
    u32 u32v = 16; ss << u32v; fss << u32v;
    i64 i64v = 17; ss << i64v; fss << i64v;
    u64 u64v = 18; ss << u64v; fss << u64v;
    fl::size sz = 19; ss << sz; fss << sz;

    // Test chaining multiple types
    ss << sc << uc << s << us << i << ui << l << ul << ll << ull;
    fss << sc << uc << s << us << i << ui << l << ul << ll << ull;

    // Test with stdint types (should match fl:: types)
    int8_t i8_std = 20; ss << i8_std; fss << i8_std;
    uint8_t u8_std = 21; ss << u8_std; fss << u8_std;
    int16_t i16_std = 22; ss << i16_std; fss << i16_std;
    uint16_t u16_std = 23; ss << u16_std; fss << u16_std;
    int32_t i32_std = 24; ss << i32_std; fss << i32_std;
    uint32_t u32_std = 25; ss << u32_std; fss << u32_std;
    int64_t i64_std = 26; ss << i64_std; fss << i64_std;
    uint64_t u64_std = 27; ss << u64_std; fss << u64_std;
}

// This file contains only compile-time tests.
// The platform-specific test functions are called to trigger
// any compile-time errors if the platform is not configured correctly.
FL_MAYBE_UNUSED static void compile_tests() {

    static_assert(fl::is_same<u32, uint32_t>::value, "u32 must be the same type as uint32_t");
    static_assert(fl::is_same<u16, uint16_t>::value, "u16 must be the same type as uint16_t");
    static_assert(fl::is_same<u8, uint8_t>::value, "u8 must be the same type as uint8_t");
    static_assert(fl::is_same<i32, int32_t>::value, "i32 must be the same type as int32_t");
    static_assert(fl::is_same<i16, int16_t>::value, "i16 must be the same type as int16_t");
    static_assert(fl::is_same<i8, int8_t>::value, "i8 must be the same type as int8_t");
    static_assert(fl::is_same<size, size_t>::value, "size must be the same type as size_t");
    static_assert(fl::is_same<uptr, uintptr_t>::value, "uptr must be the same type as uintptr_t");

    // Test StrStream integer operator overloads
    test_strstream_integer_operators();

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
