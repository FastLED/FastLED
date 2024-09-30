
// g++ --std=c++11 test.cpp

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include "doctest.h"
#include "FastLED.h"
#include "lib8tion/scale8.h"
#include "lib8tion/intmap.h"


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

TEST_CASE("scale32") {
    CHECK_EQ(scale32(0, 0), 0);
    CHECK_EQ(scale32(0, 1), 0);
    CHECK_EQ(scale32(1, 0), 0);
    CHECK_EQ(scale32(0xffffffff, 0xffffffff), 0xffffffff);
    CHECK_EQ(scale32(0xffffffff, 0xffffffff >> 1), 0xffffffff >> 1);
    CHECK_EQ(scale32(0xffffffff >> 1, 0xffffffff >> 1), 0xffffffff >> 2);

    for (int i = 0; i < 32; ++i) {
        for (int j = 0; j < 32; ++j) {
            int total_bitshift = i + j;
            if (total_bitshift > 31) {
                break;
            }
            // print out info if this test fails to capture the i,j values that are failing
            INFO("i: " << i << " j: " << j << " total_bitshift: " << total_bitshift);
            CHECK_EQ(scale32(0xffffffff >> i, 0xffffffff >> j), 0xffffffff >> total_bitshift);
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

TEST_CASE("scale32by8") {
    CHECK_EQ(scale32by8(0, 0), 0);
    CHECK_EQ(scale32by8(0, 1), 0);
    CHECK_EQ(scale32by8(1, 0), 0);
    CHECK_EQ(scale32by8(map8_to_32(1), 1), 2);
    CHECK_EQ(scale32by8(0xffffffff, 0xff), 0xffffffff);
    CHECK_EQ(scale32by8(0xffffffff, 0xff >> 1), 0xffffffff >> 1);
    CHECK_EQ(scale32by8(0xffffffff >> 1, 0xff >> 1), 0xffffffff >> 2);

    for (int i = 0; i < 32; ++i) {
        for (int j = 0; j < 8; ++j) {
            int total_bitshift = i + j;
            if (total_bitshift > 31) {
                break;
            }
            // print out info if this test fails to capture the i,j values that are failing
            INFO("i: " << i << " j: " << j << " total_bitshift: " << total_bitshift);
            CHECK_EQ(scale32by8(0xffffffff >> i, 0xff >> j), 0xffffffff >> total_bitshift);
        }
    }
}