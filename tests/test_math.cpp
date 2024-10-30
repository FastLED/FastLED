
// g++ --std=c++11 test.cpp

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include "doctest.h"
#include "FastLED.h"
#include "lib8tion/scale8.h"
#include "lib8tion/intmap.h"
#include <math.h>

#include "namespace.h"
FASTLED_USING_NAMESPACE


TEST_CASE("scale16") {
    CHECK_EQ(scale16(0, 0), 0);
    CHECK_EQ(scale16(0, 1), 0);
    CHECK_EQ(scale16(1, 0), 0);
    CHECK_EQ(scale16(0xffff, 0xffff), 0xffff);
    CHECK_EQ(scale16(0xffff, 0xffff >> 1), 0xffff >> 1);
    CHECK_EQ(scale16(0xffff >> 1, 0xffff >> 1), 0xffff >> 2);

    for (int i = 0; i < 16; ++i) {
        for (int j = 0; j < 16; ++j) {
            int total_bitshift = i + j;
            if (total_bitshift > 15) {
                break;
            }
            // print out info if this test fails to capture the i,j values that are failing
            INFO("i: " << i << " j: " << j << " total_bitshift: " << total_bitshift);
            CHECK_EQ(scale16(0xffff >> i, 0xffff >> j), 0xffff >> total_bitshift);
        }
    }
}


TEST_CASE("scale16by8") {
    CHECK_EQ(scale16by8(0, 0), 0);
    CHECK_EQ(scale16by8(0, 1), 0);
    CHECK_EQ(scale16by8(1, 0), 0);
    CHECK_EQ(scale16by8(map8_to_16(1), 1), 2);
    CHECK_EQ(scale16by8(0xffff, 0xff), 0xffff);
    CHECK_EQ(scale16by8(0xffff, 0xff >> 1), 0xffff >> 1);
    CHECK_EQ(scale16by8(0xffff >> 1, 0xff >> 1), 0xffff >> 2);

    for (int i = 0; i < 16; ++i) {
        for (int j = 0; j < 8; ++j) {
            int total_bitshift = i + j;
            if (total_bitshift > 7) {
                break;
            }
            // print out info if this test fails to capture the i,j values that are failing
            INFO("i: " << i << " j: " << j << " total_bitshift: " << total_bitshift);
            CHECK_EQ(scale16by8(0xffff >> i, 0xff >> j), 0xffff >> total_bitshift);
        }
    }
}

TEST_CASE("bit equivalence") {
    // tests that 8bit and 16bit are equivalent
    uint8_t r = 0xff;
    uint8_t r_scale = 0xff / 2;
    uint8_t brightness = 0xff / 2;
    uint16_t r_scale16 = map8_to_16(r_scale);
    uint16_t brightness16 = map8_to_16(brightness);
    uint16_t r16 = scale16by8(scale16(r_scale16, brightness16), r);
    uint8_t r8 = scale8(scale8(r_scale, brightness), r);
    CHECK_EQ(r16 >> 8, r8);
}

TEST_CASE("sqrt16") {
    float f = sqrt(.5) * 0xff;
    uint8_t result = sqrt16(map8_to_16(0xff / 2));
    CHECK_EQ(int(f), result);
    CHECK_EQ(sqrt8(0xff / 2), result);
}