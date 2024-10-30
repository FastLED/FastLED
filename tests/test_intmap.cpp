
// g++ --std=c++11 test.cpp

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include "doctest.h"
#include "lib8tion/intmap.h"
#include "namespace.h"
FASTLED_USING_NAMESPACE

TEST_CASE("map8_to_16") {
    CHECK_EQ(map8_to_16(0), 0);
    CHECK_EQ(map8_to_16(1), 0x101);
    CHECK_EQ(map8_to_16(0xff), 0xffff);
}


TEST_CASE("map8_to_32") {
    CHECK_EQ(map8_to_32(0), 0);
    CHECK_EQ(map8_to_32(1), 0x1010101);
    CHECK_EQ(map8_to_32(0xff), 0xffffffff);
}
