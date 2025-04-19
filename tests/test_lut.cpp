
// g++ --std=c++11 test.cpp

#include "test.h"

#include "test.h"
#include "lib8tion/intmap.h"
#include "fl/lut.h"


using namespace fl;

TEST_CASE("LUT interp8") {
    LUT<uint16_t> lut(2);
    uint16_t* data = lut.getDataMutable();
    data[0] = 0;
    data[1] = 255;
    CHECK_EQ(lut.interp8(0), 0);
    CHECK_EQ(lut.interp8(255), 255);
    CHECK_EQ(lut.interp8(128), 128);

    // Check the LUT interpolation for all values.
    for (uint16_t i = 0; i < 256; i++) {
        uint16_t expected = (i * 255) / 255;
        CHECK_EQ(lut.interp8(i), expected);
    }
}

TEST_CASE("LUT interp16") {
    LUT<uint16_t> lut(2);
    uint16_t* data = lut.getDataMutable();
    data[0] = 0;
    data[1] = 255;
    CHECK_EQ(lut.interp16(0), 0);
    CHECK_EQ(lut.interp16(0xffff), 255);
    CHECK_EQ(lut.interp16(0xffff / 2), 127);

    // Check the LUT interpolation for all values.
    for (int i = 0; i < 256; i++) {
        uint16_t alpha16 = map8_to_16(i);
        CHECK_EQ(i, lut.interp16(alpha16));
    }
}