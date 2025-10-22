// g++ --std=c++11 test.cpp

#include "test.h"

#include "test.h"
#include "lib8tion/intmap.h"
using namespace fl;
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

TEST_CASE("map16_to_32") {
    CHECK_EQ(map16_to_32(0), 0);
    CHECK_EQ(map16_to_32(1), 0x10001);
    CHECK_EQ(map16_to_32(0xffff), 0xffffffff);
}

TEST_CASE("map32_to_8") {
    // Zero case: 0x00000000 -> 0x00
    CHECK_EQ(map32_to_8(0x00000000), 0x00);

    // Small value: 0x01000000 (1/256th of full range) -> 0x01
    CHECK_EQ(map32_to_8(0x01000000), 0x01);

    // Quarter value: 0x40000000 (1/4 of full range) -> 0x40
    CHECK_EQ(map32_to_8(0x40000000), 0x40);

    // Half value: 0x80000000 (1/2 of full range) -> 0x80
    CHECK_EQ(map32_to_8(0x80000000), 0x80);

    // Three-quarters value: 0xC0000000 (3/4 of full range) -> 0xC0
    CHECK_EQ(map32_to_8(0xC0000000), 0xC0);

    // Boundary cases: values below 0xFE800000 round to 0xFE, 0xFE800000+ round to 0xFF
    CHECK_EQ(map32_to_8(0xFD800000), 0xFE);
    CHECK_EQ(map32_to_8(0xFE000000), 0xFE);
    CHECK_EQ(map32_to_8(0xFE800000), 0xFF);
    CHECK_EQ(map32_to_8(0xFF000000), 0xFF);

    // Maximum value: 0xFFFFFFFF -> 0xFF
    CHECK_EQ(map32_to_8(0xFFFFFFFF), 0xFF);
}
