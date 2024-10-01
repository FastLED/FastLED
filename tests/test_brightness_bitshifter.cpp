
// g++ --std=c++11 test.cpp

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include "doctest.h"
#include "lib8tion/intmap.h"
#include "lib8tion/brightness_bitshifter.h"

TEST_CASE("brightness_bitshifter") {
    SUBCASE("random test to check that the product is the same") {
        for (int i = 0; i < 10000; ++i) {
            uint8_t brightness_src = rand() % 256;
            uint8_t brightness_dst = 0b10000000 >> rand() % 6;
            uint8_t src_saved = brightness_src;
            uint8_t dst_saved = brightness_dst;
            INFO("brightness_src: " << src_saved << ", brightness_dst: " << dst_saved);
            uint16_t product = uint16_t(brightness_src) * brightness_dst;
            uint8_t shifts = brightness_bitshifter8(&brightness_src, &brightness_dst, 7);
            uint16_t new_product = uint16_t(brightness_src) * brightness_dst;  
            CHECK_EQ(product, new_product);
        }
    }
}