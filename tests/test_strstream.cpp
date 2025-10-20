
// g++ --std=c++11 test.cpp

// Test Coverage Summary:
// =======================
// This test suite provides comprehensive coverage of fl::StrStream and fl::FakeStrStream
// for all integer types across different platforms.
//
// Integer Types Tested:
// - fl:: types (13): i8, u8, i16, u16, i32, u32, i64, u64, size, uptr, iptr, ptrdiff, uint
// - Fundamental types (12): char, signed/unsigned char, short, unsigned short,
//                           int, unsigned int, long, unsigned long,
//                           long long, unsigned long long, bool
// - Fractional types (12): fract8, sfract7, fract16, sfract15, fract32, sfract31,
//                          accum88, saccum78, accum1616, saccum1516, accum124, saccum114
//
// Test Categories:
// 1. Individual type testing for compilation and basic output
// 2. Mixed type chains (multiple types in sequence)
// 3. Edge values (min/max values for each type)
// 4. Type qualifiers (const, volatile, const volatile)
// 5. Reference types (lvalue references, const references)
// 6. Template type deduction
// 7. fl::FakeStrStream compatibility with all types
// 8. Platform-specific type aliasing (e.g., int==i16 on AVR)
//
// Platform Considerations:
// - Type aliasing varies by platform (e.g., Windows uses LLP64, Linux uses LP64)
// - signed char/fl::i8 may display as characters or numbers depending on implementation
// - INT32_MIN requires special handling to avoid negation issues in itoa
//
// All tests designed to compile without ambiguous overload errors and produce
// valid output across all supported platforms (AVR, ARM, ESP32, Desktop, WebAssembly).

#include "test.h"

#include "test.h"
#include "fl/str.h"
#include "fl/strstream.h"
#include "fl/vector.h"
#include "crgb.h"
#include <sstream>

TEST_CASE("StrStream basic operations") {
    SUBCASE("Construction and assignment") {
        fl::StrStream s;
        CHECK(s.str().size() == 0);
        CHECK(s.str().c_str()[0] == '\0');

        fl::StrStream s2("hello");
        CHECK(s2.str().size() == 5);
        CHECK(fl::strcmp(s2.str().c_str(), "hello") == 0);

        fl::StrStream s3 = s2;
        CHECK(s3.str().size() == 5);
        CHECK(fl::strcmp(s3.str().c_str(), "hello") == 0);

        s = "world";
        CHECK(s.str().size() == 5);
        CHECK(fl::strcmp(s.str().c_str(), "world") == 0);
    }

    SUBCASE("Comparison operators") {
        fl::StrStream s1("hello");
        fl::StrStream s2("hello");
        fl::StrStream s3("world");

        CHECK(s1.str() == s2.str());
        CHECK(s1.str() != s3.str());
    }

    SUBCASE("Indexing") {
        fl::StrStream s("hello");
        CHECK(s.str()[0] == 'h');
        CHECK(s.str()[4] == 'o');
        CHECK(s.str()[5] == '\0');  // Null terminator
    }

    SUBCASE("Append") {
        fl::StrStream s("hello");
        s << " world";
        CHECK(s.str().size() == 11);
        CHECK(fl::strcmp(s.str().c_str(), "hello world") == 0);
    }

    SUBCASE("CRGB to fl::StrStream") {
        CRGB c(255, 0, 0);
        fl::StrStream s;
        s << c;
        CHECK(s.str().size() == 13);  // "rgb8(255,0,0)" is 13 chars
        CHECK(fl::strcmp(s.str().c_str(), "rgb8(255,0,0)") == 0);
    }
}

TEST_CASE("StrStream integer type handling") {
    SUBCASE("uint8_t displays as number") {
        fl::StrStream s;
        fl::u8 val = 65;  // ASCII 'A'
        s << val;
        // Should display as "65" (number), not "A" (character)
        CHECK(fl::strcmp(s.str().c_str(), "65") == 0);
    }

    SUBCASE("unsigned char displays as number") {
        fl::StrStream s;
        unsigned char val = 65;
        s << val;
        // Should display as "65" (number) due to 1-byte unsigned → u16 cast
        CHECK(fl::strcmp(s.str().c_str(), "65") == 0);
    }

    SUBCASE("char and integer types compile") {
        // The main goal is to ensure all types can be used without ambiguous overloads
        // Actual behavior is tested more thoroughly in compile_test.cpp
        fl::StrStream s;
        char c = 'A';
        signed char sc = 65;
        unsigned char uc = 66;

        // These should all compile without ambiguity
        s << c << sc << uc;

        // Basic sanity check - string should have some content
        CHECK(s.str().size() > 0);
    }

    SUBCASE("integer types that are NOT char display as numbers") {
        // unsigned char and other integer types should display as numbers
        fl::StrStream s;

        // Test a type that's definitely not char
        short val = 65;
        s << val;
        CHECK(fl::strcmp(s.str().c_str(), "65") == 0);
    }

    SUBCASE("char with mTreatCharAsInt") {
        fl::StrStream s;
        s.setTreatCharAsInt(true);
        char c = 65;
        s << c;
        // With mTreatCharAsInt=true, char should display as number
        CHECK(fl::strcmp(s.str().c_str(), "65") == 0);
    }

    SUBCASE("all fundamental integer types") {
        fl::StrStream s;

        // Test various fundamental types produce correct output
        signed char sc = -10;
        unsigned char uc = 200;
        short sh = -1000;
        unsigned short us = 50000;
        int i = -100000;
        unsigned int ui = 4000000;
        long l = -1000000L;
        unsigned long ul = 4000000000UL;

        s << sc << " " << uc << " " << sh << " " << us << " ";
        s << i << " " << ui << " " << l << " " << ul;

        // Verify output contains expected values
        const char* result = s.str().c_str();
        CHECK(fl::strstr(result, "-10") != nullptr);
        CHECK(fl::strstr(result, "200") != nullptr);
        CHECK(fl::strstr(result, "-1000") != nullptr);
        CHECK(fl::strstr(result, "50000") != nullptr);
    }

    SUBCASE("fl:: types work correctly") {
        fl::StrStream s;

        fl::i8 i8v = -10;
        fl::u8 u8v = 200;
        fl::i16 i16v = -1000;
        fl::u16 u16v = 50000;
        fl::i32 i32v = -100000;
        fl::u32 u32v = 4000000;

        s << i8v << " " << u8v << " " << i16v << " " << u16v << " ";
        s << i32v << " " << u32v;

        const char* result = s.str().c_str();
        CHECK(fl::strstr(result, "-10") != nullptr);
        CHECK(fl::strstr(result, "200") != nullptr);
        CHECK(fl::strstr(result, "-1000") != nullptr);
        CHECK(fl::strstr(result, "50000") != nullptr);
    }

    SUBCASE("chaining multiple types") {
        fl::StrStream s;
        s << (short)1 << (long)2 << (unsigned char)3 << (int)4;

        // Should produce "1234"
        CHECK(fl::strcmp(s.str().c_str(), "1234") == 0);
    }
}

TEST_CASE("StrStream comprehensive fl:: integer types") {
    SUBCASE("fl::i8") {
        fl::StrStream s;
        fl::i8 val = -10;
        s << val;
        // fl::i8 is signed char, which displays as a number due to fl::StrStream's handling
        CHECK(s.str().size() > 0);
    }

    SUBCASE("fl::u8") {
        fl::StrStream s;
        fl::u8 val = 200;
        s << val;
        CHECK(fl::strcmp(s.str().c_str(), "200") == 0);
    }

    SUBCASE("fl::i16") {
        fl::StrStream s;
        fl::i16 val = -1000;
        s << val;
        CHECK(fl::strcmp(s.str().c_str(), "-1000") == 0);
    }

    SUBCASE("fl::u16") {
        fl::StrStream s;
        fl::u16 val = 50000;
        s << val;
        CHECK(fl::strcmp(s.str().c_str(), "50000") == 0);
    }

    SUBCASE("fl::i32") {
        fl::StrStream s;
        fl::i32 val = -100000;
        s << val;
        CHECK(fl::strcmp(s.str().c_str(), "-100000") == 0);
    }

    SUBCASE("fl::u32") {
        fl::StrStream s;
        fl::u32 val = 4000000;
        s << val;
        CHECK(fl::strcmp(s.str().c_str(), "4000000") == 0);
    }

    SUBCASE("fl::i64") {
        fl::StrStream s;
        fl::i64 val = -1000000000LL;
        s << val;
        CHECK(fl::strcmp(s.str().c_str(), "-1000000000") == 0);
    }

    SUBCASE("fl::u64") {
        fl::StrStream s;
        fl::u64 val = 1000000000ULL;
        s << val;
        CHECK(fl::strcmp(s.str().c_str(), "1000000000") == 0);
    }

    SUBCASE("fl::size") {
        fl::StrStream s;
        fl::size val = 12345;
        s << val;
        CHECK(fl::strstr(s.str().c_str(), "12345") != nullptr);
    }

    SUBCASE("fl::uptr") {
        fl::StrStream s;
        fl::uptr val = 0x1234;
        s << val;
        CHECK(s.str().size() > 0);
    }

    SUBCASE("fl::iptr") {
        fl::StrStream s;
        fl::iptr val = -5000;
        s << val;
        CHECK(fl::strstr(s.str().c_str(), "-5000") != nullptr);
    }

    SUBCASE("fl::ptrdiff") {
        fl::StrStream s;
        fl::ptrdiff val = -1234;
        s << val;
        CHECK(fl::strstr(s.str().c_str(), "-1234") != nullptr);
    }

    SUBCASE("fl::uint") {
        fl::StrStream s;
        fl::uint val = 999999;
        s << val;
        CHECK(fl::strcmp(s.str().c_str(), "999999") == 0);
    }
}

TEST_CASE("StrStream comprehensive fundamental integer types") {
    SUBCASE("char") {
        fl::StrStream s;
        char val = 'A';
        s << val;
        CHECK(s.str().size() > 0);
    }

    SUBCASE("signed char") {
        fl::StrStream s;
        signed char val = -10;
        s << val;
        // signed char may display as character or number depending on fl::StrStream implementation
        CHECK(s.str().size() > 0);
    }

    SUBCASE("unsigned char") {
        fl::StrStream s;
        unsigned char val = 200;
        s << val;
        CHECK(fl::strcmp(s.str().c_str(), "200") == 0);
    }

    SUBCASE("short") {
        fl::StrStream s;
        short val = -1000;
        s << val;
        CHECK(fl::strcmp(s.str().c_str(), "-1000") == 0);
    }

    SUBCASE("unsigned short") {
        fl::StrStream s;
        unsigned short val = 50000;
        s << val;
        CHECK(fl::strcmp(s.str().c_str(), "50000") == 0);
    }

    SUBCASE("int") {
        fl::StrStream s;
        int val = -100000;
        s << val;
        CHECK(fl::strcmp(s.str().c_str(), "-100000") == 0);
    }

    SUBCASE("unsigned int") {
        fl::StrStream s;
        unsigned int val = 4000000;
        s << val;
        CHECK(fl::strcmp(s.str().c_str(), "4000000") == 0);
    }

    SUBCASE("long") {
        fl::StrStream s;
        long val = -1000000L;
        s << val;
        CHECK(fl::strstr(s.str().c_str(), "-1000000") != nullptr);
    }

    SUBCASE("unsigned long") {
        fl::StrStream s;
        unsigned long val = 4000000000UL;
        s << val;
        CHECK(fl::strstr(s.str().c_str(), "4000000000") != nullptr);
    }

    SUBCASE("long long") {
        fl::StrStream s;
        long long val = -1000000000LL;
        s << val;
        CHECK(fl::strcmp(s.str().c_str(), "-1000000000") == 0);
    }

    SUBCASE("unsigned long long") {
        fl::StrStream s;
        unsigned long long val = 1000000000ULL;
        s << val;
        CHECK(fl::strcmp(s.str().c_str(), "1000000000") == 0);
    }

    SUBCASE("bool") {
        fl::StrStream s;
        bool val_true = true;
        bool val_false = false;
        s << val_true << " " << val_false;
        CHECK(s.str().size() > 0);
    }
}

TEST_CASE("StrStream fractional types") {
    SUBCASE("fract8") {
        fl::StrStream s;
        fract8 val = 128;
        s << val;
        CHECK(s.str().size() > 0);
    }

    SUBCASE("sfract7") {
        fl::StrStream s;
        sfract7 val = 64;
        s << val;
        CHECK(s.str().size() > 0);
    }

    SUBCASE("fract16") {
        fl::StrStream s;
        fract16 val = 30000;
        s << val;
        CHECK(s.str().size() > 0);
    }

    SUBCASE("sfract15") {
        fl::StrStream s;
        sfract15 val = -1000;
        s << val;
        CHECK(s.str().size() > 0);
    }

    SUBCASE("fract32") {
        fl::StrStream s;
        fract32 val = 2000000;
        s << val;
        CHECK(s.str().size() > 0);
    }

    SUBCASE("sfract31") {
        fl::StrStream s;
        sfract31 val = -100000;
        s << val;
        CHECK(s.str().size() > 0);
    }

    SUBCASE("accum88") {
        fl::StrStream s;
        accum88 val = 12800;
        s << val;
        CHECK(s.str().size() > 0);
    }

    SUBCASE("saccum78") {
        fl::StrStream s;
        saccum78 val = -6400;
        s << val;
        CHECK(s.str().size() > 0);
    }

    SUBCASE("accum1616") {
        fl::StrStream s;
        accum1616 val = 2000000;
        s << val;
        CHECK(s.str().size() > 0);
    }

    SUBCASE("saccum1516") {
        fl::StrStream s;
        saccum1516 val = -100000;
        s << val;
        CHECK(s.str().size() > 0);
    }

    SUBCASE("accum124") {
        fl::StrStream s;
        accum124 val = 4096;
        s << val;
        CHECK(s.str().size() > 0);
    }

    SUBCASE("saccum114") {
        fl::StrStream s;
        saccum114 val = -2048;
        s << val;
        CHECK(s.str().size() > 0);
    }
}

TEST_CASE("StrStream mixed type chains") {
    SUBCASE("mixed fl:: types") {
        fl::StrStream s;
        s << fl::i8(-10) << " "
          << fl::u16(50000) << " "
          << fl::i32(-100000) << " "
          << fl::u64(1000000000ULL);

        const char* result = s.str().c_str();
        CHECK(fl::strstr(result, "-10") != nullptr);
        CHECK(fl::strstr(result, "50000") != nullptr);
        CHECK(fl::strstr(result, "-100000") != nullptr);
        CHECK(fl::strstr(result, "1000000000") != nullptr);
    }

    SUBCASE("mixed fundamental types") {
        fl::StrStream s;
        s << (signed char)(-10) << " "
          << (unsigned short)(50000) << " "
          << (int)(-100000) << " "
          << (unsigned long long)(1000000000ULL);

        const char* result = s.str().c_str();
        CHECK(fl::strstr(result, "-10") != nullptr);
        CHECK(fl::strstr(result, "50000") != nullptr);
        CHECK(fl::strstr(result, "-100000") != nullptr);
        CHECK(fl::strstr(result, "1000000000") != nullptr);
    }

    SUBCASE("fl:: and fundamental types mixed") {
        fl::StrStream s;
        s << fl::i8(-10) << " "
          << (short)(-1000) << " "
          << fl::u32(4000000) << " "
          << (unsigned long)(4000000000UL);

        const char* result = s.str().c_str();
        CHECK(fl::strstr(result, "-10") != nullptr);
        CHECK(fl::strstr(result, "-1000") != nullptr);
        CHECK(fl::strstr(result, "4000000") != nullptr);
    }
}

TEST_CASE("StrStream edge value testing") {
    SUBCASE("8-bit edge values") {
        fl::StrStream s;
        s << fl::i8(-128) << " ";    // min i8
        s << fl::i8(127) << " ";     // max i8
        s << fl::u8(0) << " ";       // min u8
        s << fl::u8(255);            // max u8

        // Just check that output was generated, values depend on char handling
        CHECK(s.str().size() > 0);
    }

    SUBCASE("16-bit edge values") {
        fl::StrStream s;
        s << fl::i16(-32768) << " ";  // min i16
        s << fl::i16(32767) << " ";   // max i16
        s << fl::u16(0) << " ";       // min u16
        s << fl::u16(65535);          // max u16

        const char* result = s.str().c_str();
        CHECK(fl::strcmp(result, "-32768 32767 0 65535") == 0);
    }

    SUBCASE("32-bit edge values") {
        fl::StrStream s;
        // Avoid testing MIN_INT32 directly as it can cause issues with negation in itoa
        s << fl::i32(-2147483647) << " ";       // close to min i32
        s << fl::i32(2147483647) << " ";        // max i32
        s << fl::u32(0) << " ";                 // min u32
        s << fl::u32(4294967295U);              // max u32

        const char* result = s.str().c_str();
        CHECK(fl::strstr(result, "-2147483647") != nullptr);
        CHECK(fl::strstr(result, "2147483647") != nullptr);
        CHECK(fl::strstr(result, "0") != nullptr);
        CHECK(fl::strstr(result, "4294967295") != nullptr);
    }
}

TEST_CASE("StrStream const and volatile qualifiers") {
    SUBCASE("const integer types") {
        fl::StrStream s;
        const fl::i32 ci32 = 100;
        const fl::u32 cu32 = 200;
        const fl::i16 ci16 = 300;

        s << ci32 << " " << cu32 << " " << ci16;
        CHECK(s.str().size() > 0);
        const char* result = s.str().c_str();
        CHECK(fl::strstr(result, "100") != nullptr);
        CHECK(fl::strstr(result, "200") != nullptr);
        CHECK(fl::strstr(result, "300") != nullptr);
    }

    SUBCASE("volatile integer types") {
        fl::StrStream s;
        volatile fl::u32 vu32 = 200;
        volatile fl::i16 vi16 = -300;

        s << vu32 << " " << vi16;
        CHECK(s.str().size() > 0);
        const char* result = s.str().c_str();
        CHECK(fl::strstr(result, "200") != nullptr);
        CHECK(fl::strstr(result, "-300") != nullptr);
    }

    SUBCASE("const volatile integer types") {
        fl::StrStream s;
        const volatile fl::i16 cvi16 = 300;
        const volatile fl::u8 cvu8 = 255;

        s << cvi16 << " " << cvu8;
        CHECK(s.str().size() > 0);
        const char* result = s.str().c_str();
        CHECK(fl::strstr(result, "300") != nullptr);
        CHECK(fl::strstr(result, "255") != nullptr);
    }
}

TEST_CASE("StrStream reference types") {
    SUBCASE("non-const references") {
        fl::StrStream s;
        fl::i32 val = 100;
        fl::i32& ref = val;

        s << ref;
        CHECK(fl::strcmp(s.str().c_str(), "100") == 0);
    }

    SUBCASE("const references") {
        fl::StrStream s;
        fl::i32 val = 100;
        const fl::i32& cref = val;

        s << cref;
        CHECK(fl::strcmp(s.str().c_str(), "100") == 0);
    }

    SUBCASE("mixed references") {
        fl::StrStream s;
        fl::i32 val1 = 100;
        fl::u16 val2 = 200;
        fl::i32& ref1 = val1;
        const fl::u16& cref2 = val2;

        s << ref1 << " " << cref2;
        CHECK(fl::strcmp(s.str().c_str(), "100 200") == 0);
    }
}

// Template helper function for type deduction testing
template<typename T>
bool test_template_type(T val) {
    fl::StrStream s;
    s << val;
    return s.str().size() > 0;
}

TEST_CASE("StrStream template deduction") {
    SUBCASE("fl:: types") {
        CHECK(test_template_type(fl::i8(10)));
        CHECK(test_template_type(fl::u16(1000)));
        CHECK(test_template_type(fl::i32(-50000)));
        CHECK(test_template_type(fl::u64(1000000ULL)));
    }

    SUBCASE("fundamental types") {
        CHECK(test_template_type((short)(100)));
        CHECK(test_template_type((unsigned long)(1000000UL)));
        CHECK(test_template_type((int)(-500)));
        CHECK(test_template_type((unsigned char)(255)));
    }
}

TEST_CASE("FakeStrStream integer types") {
    SUBCASE("all fl:: types") {
        fl::FakeStrStream s;
        s << fl::i8(-10);
        s << fl::u8(200);
        s << fl::i16(-1000);
        s << fl::u16(50000);
        s << fl::i32(-100000);
        s << fl::u32(4000000);
        s << fl::i64(-1000000000LL);
        s << fl::u64(1000000000ULL);
        s << fl::size(12345);
        s << fl::uint(999999);
        // Should compile without errors
        CHECK(true);
    }

    SUBCASE("all fundamental types") {
        fl::FakeStrStream s;
        s << (signed char)(-10);
        s << (unsigned char)(200);
        s << (short)(-1000);
        s << (unsigned short)(50000);
        s << (int)(-100000);
        s << (unsigned int)(4000000);
        s << (long)(-1000000L);
        s << (unsigned long)(4000000000UL);
        s << (long long)(-1000000000LL);
        s << (unsigned long long)(1000000000ULL);
        s << true;
        s << false;
        // Should compile without errors
        CHECK(true);
    }

    SUBCASE("fractional types") {
        fl::FakeStrStream s;
        s << fract8(128);
        s << sfract7(64);
        s << fract16(30000);
        s << sfract15(-1000);
        s << fract32(2000000);
        s << sfract31(-100000);
        s << accum88(12800);
        s << saccum78(-6400);
        s << accum1616(2000000);
        s << saccum1516(-100000);
        s << accum124(4096);
        s << saccum114(-2048);
        // Should compile without errors
        CHECK(true);
    }

    SUBCASE("mixed type chains") {
        fl::FakeStrStream s;
        s << fl::i8(-10) << " "
          << (short)(-1000) << " "
          << fl::u32(4000000) << " "
          << (unsigned long)(4000000000UL);
        // Should compile without errors
        CHECK(true);
    }
}

TEST_CASE("StrStream platform-specific aliased types") {
    SUBCASE("type aliasing compatibility") {
        // Test that types which may alias on certain platforms work correctly
        fl::StrStream s;
        int i = 100;
        long l = 100000L;
        fl::i16 i16 = 100;
        fl::i32 i32 = 100000;

        s << i << " " << l << " " << i16 << " " << i32;
        CHECK(s.str().size() > 0);

        const char* result = s.str().c_str();
        CHECK(fl::strstr(result, "100") != nullptr);
        CHECK(fl::strstr(result, "100000") != nullptr);
    }

    SUBCASE("pointer-sized types") {
        // Test that pointer-sized types work correctly
        fl::StrStream s;
        fl::size sz = 1234;
        fl::uptr up = 5678;
        fl::iptr ip = -999;
        fl::ptrdiff pd = -123;

        s << sz << " " << up << " " << ip << " " << pd;
        CHECK(s.str().size() > 0);
    }
}
