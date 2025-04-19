
// g++ --std=c++11 test.cpp

#include "test.h"

#include "test.h"
#include "lib8tion/intmap.h"
#include "fl/lut.h"


using namespace fl;

TEST_CASE("LUT interp") {
    LUT<uint16_t> lut(2);
    uint16_t* data = lut.getData();
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