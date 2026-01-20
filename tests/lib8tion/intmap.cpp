// g++ --std=c++11 test.cpp

#include "fl/stl/stdint.h"
#include "doctest.h"
#include "platforms/intmap.h"
using namespace fl;

TEST_CASE("map8_to_16") {
    CHECK_EQ(int_scale<uint8_t, uint16_t>(uint8_t(0)), 0);
    CHECK_EQ(int_scale<uint8_t, uint16_t>(uint8_t(1)), 0x101);
    CHECK_EQ(int_scale<uint8_t, uint16_t>(uint8_t(0xff)), 0xffff);
}

TEST_CASE("smap8_to_16") {
    // Zero case
    CHECK_EQ(int_scale<int8_t, int16_t>(int8_t(0)), 0);

    // Positive cases
    CHECK_EQ(int_scale<int8_t, int16_t>(int8_t(1)), 0x101);
    CHECK_EQ(int_scale<int8_t, int16_t>(int8_t(127)), 0x7F7F);  // 127 * 0x0101 = 32639

    // Negative cases: bit pattern is replicated
    CHECK_EQ(int_scale<int8_t, int16_t>(int8_t(-1)), int16_t(-1));        // 0xFF -> 0xFFFF = -1
    CHECK_EQ(int_scale<int8_t, int16_t>(int8_t(-128)), int16_t(-32640));  // 0x80 -> 0x8080 = -32640
}


TEST_CASE("map8_to_32") {
    CHECK_EQ(int_scale<uint8_t, uint32_t>(uint8_t(0)), 0);
    CHECK_EQ(int_scale<uint8_t, uint32_t>(uint8_t(1)), 0x1010101);
    CHECK_EQ(int_scale<uint8_t, uint32_t>(uint8_t(0xff)), 0xffffffff);
}

TEST_CASE("smap8_to_32") {
    // Zero case
    CHECK_EQ(int_scale<int8_t, int32_t>(int8_t(0)), 0);

    // Positive cases
    CHECK_EQ(int_scale<int8_t, int32_t>(int8_t(1)), 0x1010101);
    CHECK_EQ(int_scale<int8_t, int32_t>(int8_t(127)), 0x7F7F7F7F);  // 127 * 0x01010101

    // Negative cases: bit pattern is replicated
    CHECK_EQ(int_scale<int8_t, int32_t>(int8_t(-1)), int32_t(-1));              // 0xFF -> 0xFFFFFFFF = -1
    CHECK_EQ(int_scale<int8_t, int32_t>(int8_t(-128)), int32_t(-2139062144));   // 0x80 -> 0x80808080 = -2139062144
}

TEST_CASE("map16_to_32") {
    CHECK_EQ(int_scale<uint16_t, uint32_t>(uint16_t(0)), 0);
    CHECK_EQ(int_scale<uint16_t, uint32_t>(uint16_t(1)), 0x10001);
    CHECK_EQ(int_scale<uint16_t, uint32_t>(uint16_t(0xffff)), 0xffffffff);
}

TEST_CASE("smap16_to_32") {
    // Zero case
    CHECK_EQ(int_scale<int16_t, int32_t>(int16_t(0)), 0);

    // Positive cases
    CHECK_EQ(int_scale<int16_t, int32_t>(int16_t(1)), 0x10001);
    CHECK_EQ(int_scale<int16_t, int32_t>(int16_t(32767)), 0x7FFF7FFF);  // 32767 * 0x00010001

    // Negative cases: bit pattern is replicated
    CHECK_EQ(int_scale<int16_t, int32_t>(int16_t(-1)), int32_t(-1));              // 0xFFFF -> 0xFFFFFFFF = -1
    CHECK_EQ(int_scale<int16_t, int32_t>(int16_t(-32768)), int32_t(-2147450880)); // 0x8000 -> 0x80008000 = -2147450880
}

TEST_CASE("map16_to_8") {
    // Zero case: 0x0000 -> 0x00
    CHECK_EQ(int_scale<uint16_t, uint8_t>(uint16_t(0x0000)), 0x00);

    // Small value: 0x0100 (1/256th of full range) -> 0x01
    CHECK_EQ(int_scale<uint16_t, uint8_t>(uint16_t(0x0100)), 0x01);

    // Quarter value: 0x4000 (1/4 of full range) -> 0x40
    CHECK_EQ(int_scale<uint16_t, uint8_t>(uint16_t(0x4000)), 0x40);

    // Half value: 0x8000 (1/2 of full range) -> 0x80
    CHECK_EQ(int_scale<uint16_t, uint8_t>(uint16_t(0x8000)), 0x80);

    // Three-quarters value: 0xC000 (3/4 of full range) -> 0xC0
    CHECK_EQ(int_scale<uint16_t, uint8_t>(uint16_t(0xC000)), 0xC0);

    // Boundary cases: values below 0xFE80 round to 0xFE, 0xFE80+ round to 0xFF
    CHECK_EQ(int_scale<uint16_t, uint8_t>(uint16_t(0xFD80)), 0xFE);
    CHECK_EQ(int_scale<uint16_t, uint8_t>(uint16_t(0xFE00)), 0xFE);
    CHECK_EQ(int_scale<uint16_t, uint8_t>(uint16_t(0xFE80)), 0xFF);
    CHECK_EQ(int_scale<uint16_t, uint8_t>(uint16_t(0xFF00)), 0xFF);

    // Maximum value: 0xFFFF -> 0xFF
    CHECK_EQ(int_scale<uint16_t, uint8_t>(uint16_t(0xFFFF)), 0xFF);
}

TEST_CASE("smap16_to_8") {
    // Zero case
    CHECK_EQ(int_scale<int16_t, int8_t>(int16_t(0)), 0);

    // Positive cases
    CHECK_EQ(int_scale<int16_t, int8_t>(int16_t(256)), 1);    // 0x0100 -> 0x01
    CHECK_EQ(int_scale<int16_t, int8_t>(int16_t(0x4000)), 0x40);  // Quarter -> 0x40

    // Saturation case at positive extreme
    CHECK_EQ(int_scale<int16_t, int8_t>(int16_t(0x7f80)), 127);   // Saturate to max positive
    CHECK_EQ(int_scale<int16_t, int8_t>(int16_t(32767)), 127);    // Max positive value

    // Negative cases
    CHECK_EQ(int_scale<int16_t, int8_t>(int16_t(-256)), -1);       // -256 -> -1
    CHECK_EQ(int_scale<int16_t, int8_t>(int16_t(-128)), 0);        // Small negative rounds toward 0
    CHECK_EQ(int_scale<int16_t, int8_t>(int16_t(-32640)), -127);   // (-32640 + 128) >> 8 = -127
    CHECK_EQ(int_scale<int16_t, int8_t>(int16_t(-32768)), -128);   // (-32768 + 128) >> 8 = -128
}

TEST_CASE("map32_to_16") {
    // Zero case: 0x00000000 -> 0x0000
    CHECK_EQ(int_scale<uint32_t, uint16_t>(uint32_t(0x00000000)), 0x0000);

    // Small value: 0x00010000 (1/65536th of full range) -> 0x0001
    CHECK_EQ(int_scale<uint32_t, uint16_t>(uint32_t(0x00010000)), 0x0001);

    // Quarter value: 0x40000000 (1/4 of full range) -> 0x4000
    CHECK_EQ(int_scale<uint32_t, uint16_t>(uint32_t(0x40000000)), 0x4000);

    // Half value: 0x80000000 (1/2 of full range) -> 0x8000
    CHECK_EQ(int_scale<uint32_t, uint16_t>(uint32_t(0x80000000)), 0x8000);

    // Three-quarters value: 0xC0000000 (3/4 of full range) -> 0xC000
    CHECK_EQ(int_scale<uint32_t, uint16_t>(uint32_t(0xC0000000)), 0xC000);

    // Boundary cases: values below 0xFFFE8000 round to 0xFFFE, 0xFFFE8000+ round to 0xFFFF
    CHECK_EQ(int_scale<uint32_t, uint16_t>(uint32_t(0xFFFD8000)), 0xFFFE);
    CHECK_EQ(int_scale<uint32_t, uint16_t>(uint32_t(0xFFFE0000)), 0xFFFE);
    CHECK_EQ(int_scale<uint32_t, uint16_t>(uint32_t(0xFFFE8000)), 0xFFFF);
    CHECK_EQ(int_scale<uint32_t, uint16_t>(uint32_t(0xFFFF0000)), 0xFFFF);

    // Maximum value: 0xFFFFFFFF -> 0xFFFF
    CHECK_EQ(int_scale<uint32_t, uint16_t>(uint32_t(0xFFFFFFFF)), 0xFFFF);
}

TEST_CASE("smap32_to_16") {
    // Zero case
    CHECK_EQ(int_scale<int32_t, int16_t>(int32_t(0)), 0);

    // Positive cases
    CHECK_EQ(int_scale<int32_t, int16_t>(int32_t(0x00010000)), 0x0001);  // Small positive
    CHECK_EQ(int_scale<int32_t, int16_t>(int32_t(0x40000000)), 0x4000);  // Quarter

    // Saturation case at positive extreme
    CHECK_EQ(int_scale<int32_t, int16_t>(int32_t(0x7fff8000)), 32767);   // Saturate to max positive
    CHECK_EQ(int_scale<int32_t, int16_t>(int32_t(2147483647)), 32767);   // Max positive value

    // Negative cases
    CHECK_EQ(int_scale<int32_t, int16_t>(int32_t(-65536)), -1);           // -65536 -> -1
    CHECK_EQ(int_scale<int32_t, int16_t>(int32_t(-32768)), 0);            // Small negative rounds toward 0
    CHECK_EQ(int_scale<int32_t, int16_t>(int32_t(-2147450880)), -32767);  // (-2147450880 + 32768) >> 16 = -32767
    CHECK_EQ(int_scale<int32_t, int16_t>(int32_t(-2147483648)), -32768);  // Min negative value
}

TEST_CASE("map32_to_8") {
    // Zero case: 0x00000000 -> 0x00
    CHECK_EQ(int_scale<uint32_t, uint8_t>(uint32_t(0x00000000)), 0x00);

    // Small value: 0x01000000 (1/256th of full range) -> 0x01
    CHECK_EQ(int_scale<uint32_t, uint8_t>(uint32_t(0x01000000)), 0x01);

    // Quarter value: 0x40000000 (1/4 of full range) -> 0x40
    CHECK_EQ(int_scale<uint32_t, uint8_t>(uint32_t(0x40000000)), 0x40);

    // Half value: 0x80000000 (1/2 of full range) -> 0x80
    CHECK_EQ(int_scale<uint32_t, uint8_t>(uint32_t(0x80000000)), 0x80);

    // Three-quarters value: 0xC0000000 (3/4 of full range) -> 0xC0
    CHECK_EQ(int_scale<uint32_t, uint8_t>(uint32_t(0xC0000000)), 0xC0);

    // Boundary cases: values below 0xFE800000 round to 0xFE, 0xFE800000+ round to 0xFF
    CHECK_EQ(int_scale<uint32_t, uint8_t>(uint32_t(0xFD800000)), 0xFE);
    CHECK_EQ(int_scale<uint32_t, uint8_t>(uint32_t(0xFE000000)), 0xFE);
    CHECK_EQ(int_scale<uint32_t, uint8_t>(uint32_t(0xFE800000)), 0xFF);
    CHECK_EQ(int_scale<uint32_t, uint8_t>(uint32_t(0xFF000000)), 0xFF);

    // Maximum value: 0xFFFFFFFF -> 0xFF
    CHECK_EQ(int_scale<uint32_t, uint8_t>(uint32_t(0xFFFFFFFF)), 0xFF);
}

TEST_CASE("smap32_to_8") {
    // Zero case
    CHECK_EQ(int_scale<int32_t, int8_t>(int32_t(0)), 0);

    // Positive cases
    CHECK_EQ(int_scale<int32_t, int8_t>(int32_t(0x01000000)), 0x01);  // Small positive
    CHECK_EQ(int_scale<int32_t, int8_t>(int32_t(0x40000000)), 0x40);  // Quarter

    // Saturation case at positive extreme
    CHECK_EQ(int_scale<int32_t, int8_t>(int32_t(0x7F000000)), 127);   // Saturate to max positive
    CHECK_EQ(int_scale<int32_t, int8_t>(int32_t(2147483647)), 127);   // Max positive value

    // Negative cases
    CHECK_EQ(int_scale<int32_t, int8_t>(int32_t(-16777216)), -1);     // -16777216 -> -1
    CHECK_EQ(int_scale<int32_t, int8_t>(int32_t(-65536)), 0);         // Small negative rounds toward 0
    CHECK_EQ(int_scale<int32_t, int8_t>(int32_t(-2147418112)), -128); // (-2147418112 + 8388608) >> 24 = -128
    CHECK_EQ(int_scale<int32_t, int8_t>(int32_t(-2147483648)), -128); // Min negative value naturally maps to -128
}
