
// g++ --std=c++11 test.cpp

#include "test.h"

#include "test.h"
#include "lib8tion/intmap.h"
#include "fl/lut.h"


using namespace fl;

TEST_CASE("Json2 Tests") {

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
