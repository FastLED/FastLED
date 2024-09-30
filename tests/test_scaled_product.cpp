
// g++ --std=c++11 test.cpp

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include "doctest.h"
#include "lib8tion/scaled_product.h"

TEST_CASE("map8_to_16") {
    CHECK_EQ(map8_to_16(0), 0);
    CHECK_EQ(map8_to_16(1), 257);
    CHECK_EQ(map8_to_16(0xff), 0xffff);
}

TEST_CASE("scaled_product8_8_8") {
    CHECK_EQ(scaled_product8_8_8(0, 0), 0);
    CHECK_EQ(scaled_product8_8_8(0, 1), 0);
    CHECK_EQ(scaled_product8_8_8(1, 0), 0);
    CHECK_EQ(scaled_product8_8_8(0xff >> 1, 0xff), 0xff >> 1);
    CHECK_EQ(scaled_product8_8_8(0xff, 0xff), 0xff);


    SUBCASE("iterate") {
        for (int i = 0; i < 256; ++i) {
            CHECK_EQ(scaled_product8_8_8(i, 0xff), i);
        }
        for (int i = 0; i < 256; ++i) {
            CHECK_EQ(scaled_product8_8_8(i, 0xff >> 1), i >> 1);
        }
    }
}

#if 1
TEST_CASE("scaled_product8_8_16") {
    CHECK_EQ(scaled_product8_8_16(0, 0), 0);
    CHECK_EQ(scaled_product8_8_16(0, 1), 0);
    CHECK_EQ(scaled_product8_8_16(1, 0), 0);
    CHECK_EQ(scaled_product8_8_16(1, 1), 3);
    CHECK_EQ(scaled_product8_8_16(0xff, 0xff), 0xffff);
}


#endif  // 0
