#include "fl/stl/strstream.h"
#include "fl/stl/cstdint.h"
#include "fl/stl/new.h"
#include "doctest.h"
#include "fl/int.h"
#include "fl/stl/move.h"
#include "fl/stl/string.h"

TEST_CASE("StrStream handles all integer types generically") {
    fl::StrStream ss;

    SUBCASE("signed char types") {
        signed char sc = -42;
        ss << sc;
        CHECK(ss.str() == "-42");
    }

    SUBCASE("unsigned char types") {
        unsigned char uc = 200;
        ss.clear();
        ss << uc;
        CHECK(ss.str() == "200");
    }

    SUBCASE("short types") {
        short s = -12345;
        ss.clear();
        ss << s;
        CHECK(ss.str() == "-12345");

        unsigned short us = 54321;
        ss.clear();
        ss << us;
        CHECK(ss.str() == "54321");
    }

    SUBCASE("int types") {
        int i = -123456;
        ss.clear();
        ss << i;
        CHECK(ss.str() == "-123456");

        unsigned int ui = 654321;
        ss.clear();
        ss << ui;
        CHECK(ss.str() == "654321");
    }

    SUBCASE("long types") {
        long l = -1234567;
        ss.clear();
        ss << l;
        CHECK(ss.str() == "-1234567");

        unsigned long ul = 7654321;
        ss.clear();
        ss << ul;
        CHECK(ss.str() == "7654321");
    }

    SUBCASE("long long types") {
        long long ll = -123456789012345LL;
        ss.clear();
        ss << ll;
        CHECK(ss.str() == "-123456789012345");

        unsigned long long ull = 987654321098765ULL;
        ss.clear();
        ss << ull;
        CHECK(ss.str() == "987654321098765");
    }

    SUBCASE("fixed-width integer types") {
        int8_t i8 = -127;
        ss.clear();
        ss << i8;
        CHECK(ss.str() == "-127");

        uint8_t u8 = 255;
        ss.clear();
        ss << u8;
        CHECK(ss.str() == "255");

        int16_t i16 = -32000;
        ss.clear();
        ss << i16;
        CHECK(ss.str() == "-32000");

        uint16_t u16 = 65000;
        ss.clear();
        ss << u16;
        CHECK(ss.str() == "65000");

        int32_t i32 = -2000000000;
        ss.clear();
        ss << i32;
        CHECK(ss.str() == "-2000000000");

        uint32_t u32 = 4000000000U;
        ss.clear();
        ss << u32;
        CHECK(ss.str() == "4000000000");

        int64_t i64 = -9000000000000000LL;
        ss.clear();
        ss << i64;
        CHECK(ss.str() == "-9000000000000000");

        uint64_t u64 = 18000000000000000ULL;
        ss.clear();
        ss << u64;
        CHECK(ss.str() == "18000000000000000");
    }

    SUBCASE("fl:: integer types") {
        fl::i8 fi8 = -100;
        ss.clear();
        ss << fi8;
        CHECK(ss.str() == "-100");

        fl::u16 fu16 = 50000;
        ss.clear();
        ss << fu16;
        CHECK(ss.str() == "50000");

        fl::i32 fi32 = -1000000;
        ss.clear();
        ss << fi32;
        CHECK(ss.str() == "-1000000");

        fl::u32 fu32 = 3000000000U;
        ss.clear();
        ss << fu32;
        CHECK(ss.str() == "3000000000");

        fl::i64 fi64 = -5000000000000LL;
        ss.clear();
        ss << fi64;
        CHECK(ss.str() == "-5000000000000");

        fl::u64 fu64 = 10000000000000ULL;
        ss.clear();
        ss << fu64;
        CHECK(ss.str() == "10000000000000");
    }

    SUBCASE("mixed integer types in single stream") {
        ss.clear();
        ss << int16_t(-100) << " " << uint32_t(123456) << " " << int64_t(-999999999);
        CHECK(ss.str() == "-100 123456 -999999999");
    }
}

TEST_CASE("StrN write handles all integer types generically") {
    fl::StrN<64> sn;

    SUBCASE("various integer types") {
        int16_t i16 = -1000;
        sn.write(i16);
        CHECK(fl::string(sn.c_str()) == "-1000");

        sn.clear();
        uint32_t u32 = 2000000000U;
        sn.write(u32);
        CHECK(fl::string(sn.c_str()) == "2000000000");

        sn.clear();
        int64_t i64 = -3000000000000LL;
        sn.write(i64);
        CHECK(fl::string(sn.c_str()) == "-3000000000000");
    }
}

TEST_CASE("string append handles all integer types generically") {
    fl::string s;

    SUBCASE("various integer types") {
        s.append(int16_t(-5000));
        CHECK(s == "-5000");

        s.clear();
        s.append(uint32_t(4000000000U));
        CHECK(s == "4000000000");

        s.clear();
        s.append(int64_t(-7000000000000LL));
        CHECK(s == "-7000000000000");

        s.clear();
        s.append((long)123456);
        CHECK(s == "123456");

        s.clear();
        s.append((unsigned long)987654);
        CHECK(s == "987654");
    }

    SUBCASE("mixed integer types") {
        s.clear();
        s.append((short)-100);
        s.append(" ");
        s.append((unsigned int)200000);
        s.append(" ");
        s.append((long long)-300000000000LL);
        CHECK(s == "-100 200000 -300000000000");
    }
}

TEST_CASE("char types are handled correctly") {
    fl::StrStream ss;

    SUBCASE("char is treated as character by default") {
        char c = 'A';
        ss << c;
        CHECK(ss.str() == "A");
    }

    SUBCASE("char can be treated as integer with flag") {
        ss.setTreatCharAsInt(true);
        char c = 65;  // ASCII 'A'
        ss << c;
        CHECK(ss.str() == "65");
    }

    SUBCASE("signed char is treated as number") {
        signed char sc = 65;
        ss << sc;
        CHECK(ss.str() == "65");
    }

    SUBCASE("unsigned char is treated as number") {
        unsigned char uc = 65;
        ss << uc;
        CHECK(ss.str() == "65");
    }
}
