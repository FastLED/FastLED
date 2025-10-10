
// g++ --std=c++11 test.cpp

#include "test.h"

#include "test.h"
#include "fl/str.h"
#include "fl/strstream.h"
#include "fl/vector.h"
#include "crgb.h"
#include <sstream>

#include "fl/namespace.h"

using namespace fl;

TEST_CASE("StrStream basic operations") {
    SUBCASE("Construction and assignment") {
        StrStream s;
        CHECK(s.str().size() == 0);
        CHECK(s.str().c_str()[0] == '\0');

        StrStream s2("hello");
        CHECK(s2.str().size() == 5);
        CHECK(strcmp(s2.str().c_str(), "hello") == 0);

        StrStream s3 = s2;
        CHECK(s3.str().size() == 5);
        CHECK(strcmp(s3.str().c_str(), "hello") == 0);

        s = "world";
        CHECK(s.str().size() == 5);
        CHECK(strcmp(s.str().c_str(), "world") == 0);
    }

    SUBCASE("Comparison operators") {
        StrStream s1("hello");
        StrStream s2("hello");
        StrStream s3("world");

        CHECK(s1.str() == s2.str());
        CHECK(s1.str() != s3.str());
    }

    SUBCASE("Indexing") {
        StrStream s("hello");
        CHECK(s.str()[0] == 'h');
        CHECK(s.str()[4] == 'o');
        CHECK(s.str()[5] == '\0');  // Null terminator
    }

    SUBCASE("Append") {
        StrStream s("hello");
        s << " world";
        CHECK(s.str().size() == 11);
        CHECK(strcmp(s.str().c_str(), "hello world") == 0);
    }

    SUBCASE("CRGB to StrStream") {
        CRGB c(255, 0, 0);
        StrStream s;
        s << c;
        CHECK(s.str().size() == 13);  // "CRGB(255,0,0)" is 13 chars
        CHECK(strcmp(s.str().c_str(), "CRGB(255,0,0)") == 0);
    }
}

TEST_CASE("StrStream integer type handling") {
    SUBCASE("uint8_t displays as number") {
        StrStream s;
        fl::u8 val = 65;  // ASCII 'A'
        s << val;
        // Should display as "65" (number), not "A" (character)
        CHECK(strcmp(s.str().c_str(), "65") == 0);
    }

    SUBCASE("unsigned char displays as number") {
        StrStream s;
        unsigned char val = 65;
        s << val;
        // Should display as "65" (number) due to 1-byte unsigned → u16 cast
        CHECK(strcmp(s.str().c_str(), "65") == 0);
    }

    SUBCASE("char and integer types compile") {
        // The main goal is to ensure all types can be used without ambiguous overloads
        // Actual behavior is tested more thoroughly in compile_test.cpp
        StrStream s;
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
        StrStream s;

        // Test a type that's definitely not char
        short val = 65;
        s << val;
        CHECK(strcmp(s.str().c_str(), "65") == 0);
    }

    SUBCASE("char with mTreatCharAsInt") {
        StrStream s;
        s.setTreatCharAsInt(true);
        char c = 65;
        s << c;
        // With mTreatCharAsInt=true, char should display as number
        CHECK(strcmp(s.str().c_str(), "65") == 0);
    }

    SUBCASE("all fundamental integer types") {
        StrStream s;

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
        CHECK(strstr(result, "-10") != nullptr);
        CHECK(strstr(result, "200") != nullptr);
        CHECK(strstr(result, "-1000") != nullptr);
        CHECK(strstr(result, "50000") != nullptr);
    }

    SUBCASE("fl:: types work correctly") {
        StrStream s;

        fl::i8 i8v = -10;
        fl::u8 u8v = 200;
        fl::i16 i16v = -1000;
        fl::u16 u16v = 50000;
        fl::i32 i32v = -100000;
        fl::u32 u32v = 4000000;

        s << i8v << " " << u8v << " " << i16v << " " << u16v << " ";
        s << i32v << " " << u32v;

        const char* result = s.str().c_str();
        CHECK(strstr(result, "-10") != nullptr);
        CHECK(strstr(result, "200") != nullptr);
        CHECK(strstr(result, "-1000") != nullptr);
        CHECK(strstr(result, "50000") != nullptr);
    }

    SUBCASE("chaining multiple types") {
        StrStream s;
        s << (short)1 << (long)2 << (unsigned char)3 << (int)4;

        // Should produce "1234"
        CHECK(strcmp(s.str().c_str(), "1234") == 0);
    }
}
