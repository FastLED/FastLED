
// g++ --std=c++11 test.cpp

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include "doctest.h"
#include "lib8tion/intmap.h"
#include "lib8tion/brightness_bitshifter.h"
#include <iostream>
#include <bitset>

#include "namespace.h"
FASTLED_USING_NAMESPACE

TEST_CASE("brightness_bitshifter") {
    SUBCASE("random test to check that the product is the same") {
        int count = 0;
        for (int i = 0; i < 10000; ++i) {
            uint8_t brightness_src = 0b10000000 >> rand() % 6;
            uint8_t brightness_dst = rand() % 256;
            uint16_t product = uint16_t(brightness_src) * brightness_dst;
            uint8_t shifts = brightness_bitshifter8(&brightness_src, &brightness_dst, 7);
            uint16_t new_product = uint16_t(brightness_src) * brightness_dst;  
            CHECK_EQ(product, new_product);
            if (shifts) {
                count++;
            }
        }
        CHECK_GT(count, 0);
    }
}

TEST_CASE("brightness_bitshifter16") {
    SUBCASE("simple with steps=2") {
        uint8_t brightness_src = 0x1 << 1;
        uint16_t brightness_dst = 0x1 << 2;
        uint8_t max_shifts = 8;
        
        uint8_t shifts = brightness_bitshifter16(&brightness_src, &brightness_dst, max_shifts, 2);
        
        CHECK_EQ(shifts, 1);
        CHECK_EQ(brightness_src, 1);
        CHECK_EQ(brightness_dst, 0x1 << 4);
    }

    SUBCASE("simple with steps=1") {
        uint8_t brightness_src = 0x1 << 1;
        uint16_t brightness_dst = 0x1 << 1;
        uint8_t max_shifts = 8;
        
        uint8_t shifts = brightness_bitshifter16(&brightness_src, &brightness_dst, max_shifts, 1);
        
        CHECK_EQ(shifts, 1);
        CHECK_EQ(brightness_src, 1);
        CHECK_EQ(brightness_dst, 0x1 << 2);
    }

    SUBCASE("random test to check that the product is the same") {
        int count = 0;
        for (int i = 0; i < 10000; ++i) {
            uint8_t brightness_src = 0b10000000 >> (rand() % 8);
            uint16_t brightness_dst = rand() % uint32_t(65536);
            uint32_t product = uint32_t(brightness_src >> 8) * brightness_dst;
            uint8_t max_shifts = 8;
            uint8_t steps = 2;
            
            uint8_t shifts = brightness_bitshifter16(&brightness_src, &brightness_dst, max_shifts, steps);
            
            uint32_t new_product = uint32_t(brightness_src >> 8) * brightness_dst;  
            CHECK_EQ(product, new_product);
            if (shifts) {
                count++;
            }
        }
        CHECK_GT(count, 0);
    }
}
