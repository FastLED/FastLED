
// g++ --std=c++11 test.cpp


#include "fl/math/intmap.h"
#include "fl/math/transform.h"
#include "fl/stl/stdint.h"
#include "test.h"
#include "fl/math/geometry.h"
#include "fl/math/math.h"

FL_TEST_FILE(FL_FILEPATH) {



FL_TEST_CASE("Transform16::ToBounds(max_value)") {
    // fl::Transform16 tx = fl::Transform16::ToBounds(255);

    FL_SUBCASE("Check bounds at 128") {
        // known bad at i == 128
        fl::Transform16 tx = fl::Transform16::ToBounds(255);
        fl::alpha16 i16 = fl::map8_to_16(128);
        fl::vec2<fl::alpha16> xy_input = fl::vec2<fl::alpha16>(i16, i16);
        fl::vec2<fl::alpha16> xy = tx.transform(xy_input);
        FL_DINFO("i = " << 128);
        FL_REQUIRE_EQ(128, (uint16_t)xy.x);
        FL_REQUIRE_EQ(128, (uint16_t)xy.y);
    }

    FL_SUBCASE("Check identity from 8 -> 16") {
        fl::Transform16 tx = fl::Transform16::ToBounds(255);
        for (uint16_t i = 0; i < 256; i++) {
            fl::alpha16 i16 = fl::map8_to_16(i);
            fl::vec2<fl::alpha16> xy_input = fl::vec2<fl::alpha16>(i16, i16);
            fl::vec2<fl::alpha16> xy = tx.transform(xy_input);
            FL_DINFO("i = " << i);
            FL_REQUIRE_EQ(i, (uint16_t)xy.x);
            FL_REQUIRE_EQ(i, (uint16_t)xy.y);
        }
    }

    FL_SUBCASE("Check all bounds are in 255") {
        fl::Transform16 tx = fl::Transform16::ToBounds(255);
        uint32_t smallest = ~0;
        uint32_t largest = 0;
        for (uint16_t i = 0; i < 256; i++) {
            fl::alpha16 i16 = fl::map8_to_16(i);
            fl::vec2<fl::alpha16> xy_input = fl::vec2<fl::alpha16>(i16, i16);
            fl::vec2<fl::alpha16> xy = tx.transform(xy_input);
            FL_DINFO("i = " << i);
            FL_REQUIRE_LE((uint16_t)xy.x, 255);
            FL_REQUIRE_LE((uint16_t)xy.y, 255);
            smallest = fl::min(smallest, (uint32_t)(uint16_t)xy.x);
            largest = fl::max(largest, (uint32_t)(uint16_t)xy.x);
        }

        FL_REQUIRE_EQ(0, smallest);
        FL_REQUIRE_EQ(255, largest);
    }
}

FL_TEST_CASE("Transform16::ToBounds(min, max)") {
    FL_SUBCASE("Check bounds at 128") {
        fl::alpha16 low = 127;
        fl::alpha16 high = 255 + 127;
        fl::vec2<fl::alpha16> min = fl::vec2<fl::alpha16>(low, low);
        fl::vec2<fl::alpha16> max = fl::vec2<fl::alpha16>(high, high);
        fl::Transform16 tx = fl::Transform16::ToBounds(min, max);
        auto t1 = tx.transform(fl::vec2<fl::alpha16>(0, 0));
        auto t2 = tx.transform(fl::vec2<fl::alpha16>(0xffff, 0xffff));
        FL_REQUIRE_EQ(fl::vec2<fl::alpha16>(low, low), t1);
        FL_REQUIRE_EQ(fl::vec2<fl::alpha16>(high, high), t2);
    }
}

} // FL_TEST_FILE
