
// g++ --std=c++11 test.cpp

#include "test.h"

#include "test.h"
#include "lib8tion/intmap.h"
#include "lib8tion/brightness_bitshifter.h"
#include <iostream>
#include <bitset>

#include "fl/namespace.h"
FASTLED_USING_NAMESPACE

TEST_CASE("brightness_bitshifter8") {
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

    SUBCASE("fixed test data mimicing actual use") {
        const uint8_t test_data[][2][2] = {
            // brightness_bitshifter8 is always called with brightness_src = 0b00010000
            { // test case
                // src       dst
                {0b00010000, 0b00000000}, // input
                {0b00010000, 0b00000000}, // output
            },
            {
                {0b00010000, 0b00000001},
                {0b00000001, 0b00010000},
            },
            {
                {0b00010000, 0b00000100},
                {0b00000001, 0b01000000},
            },
            {
                {0b00010000, 0b00010000},
                {0b00000010, 0b10000000},
            },
            {
                {0b00010000, 0b00001010},
                {0b00000001, 0b10100000},
            },
            {
                {0b00010000, 0b00101010},
                {0b00000100, 0b10101000},
            },
            {
                {0b00010000, 0b11101010},
                {0b00010000, 0b11101010},
            },
        };

        for (const auto& data : test_data) {
            uint8_t brightness_src = data[0][0];
            uint8_t brightness_dst = data[0][1];
            uint8_t shifts = brightness_bitshifter8(&brightness_src, &brightness_dst, 4);
            INFO("input  brightness_src: ", data[0][0], " ; input brightness_dst: ", data[0][1]);
            INFO("output brightness_src: ", brightness_src, " ; output brightness_dst: ", brightness_dst);
            INFO("shifts : ", shifts);
            CHECK_EQ(brightness_src, data[1][0]);
            CHECK_EQ(brightness_dst, data[1][1]);
        }
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

    SUBCASE("fixed test data mimicing actual use") {
        const uint16_t test_data[][2][2] = {
            // brightness_bitshifter16 is always called with brightness_src between 0b00000001 - 0b00010000
            { // test case
                // src       dst
                {0b00000001, 0b0000000000000000}, // input
                {0b00000001, 0b0000000000000000}, // output
            },
            {
                {0b00000001, 0b0000000000000001},
                {0b00000001, 0b0000000000000001},
            },
            {
                {0b00000001, 0b0000000000000010},
                {0b00000001, 0b0000000000000010},
            },
            {
                {0b00000010, 0b0000000000000001},
                {0b00000001, 0b0000000000000100},
            },
            {
                {0b00001010, 0b0000000000001010},
                {0b00000101, 0b0000000000101000},
            },
            {
                {0b00010000, 0b0000111000100100},
                {0b00000100, 0b1110001001000000},
            },
            {
                {0b00010000, 0b0011100010010010},
                {0b00001000, 0b1110001001001000},
            },
            {
                {0b00010000, 0b0110001001001110},
                {0b00010000, 0b0110001001001110},
            },
            {
                {0b00010000, 0b1110001001001110},
                {0b00010000, 0b1110001001001110},
            },
        };

        for (const auto& data : test_data) {
            uint8_t brightness_src = static_cast<uint8_t>(data[0][0]);
            uint16_t brightness_dst = data[0][1];
            uint8_t shifts = brightness_bitshifter16(&brightness_src, &brightness_dst, 4, 2);
            INFO("input  brightness_src: ", data[0][0], " ; input brightness_dst: ", data[0][1]);
            INFO("output brightness_src: ", brightness_src, " ; output brightness_dst: ", brightness_dst);
            INFO("shifts (by 2 bits): ", shifts);
            CHECK_EQ(brightness_src, data[1][0]);
            CHECK_EQ(brightness_dst, data[1][1]);
        }
    }
}
