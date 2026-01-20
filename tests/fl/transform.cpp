
// g++ --std=c++11 test.cpp


#include "lib8tion/intmap.h"
#include "fl/transform.h"
#include "fl/stl/stdint.h"
#include "doctest.h"
#include "fl/geometry.h"
#include "fl/math_macros.h"



TEST_CASE("Transform16::ToBounds(max_value)") {
    // fl::Transform16 tx = fl::Transform16::ToBounds(255);

    SUBCASE("Check bounds at 128") {
        // known bad at i == 128
        fl::Transform16 tx = fl::Transform16::ToBounds(255);
        uint16_t i16 = fl::map8_to_16(128);
        fl::vec2<uint16_t> xy_input = fl::vec2<uint16_t>(i16, i16);
        fl::vec2<uint16_t> xy = tx.transform(xy_input);
        INFO("i = " << 128);
        REQUIRE_EQ(128, xy.x);
        REQUIRE_EQ(128, xy.y);
    }

    SUBCASE("Check identity from 8 -> 16") {
        fl::Transform16 tx = fl::Transform16::ToBounds(255);
        for (uint16_t i = 0; i < 256; i++) {
            uint16_t i16 = fl::map8_to_16(i);
            fl::vec2<uint16_t> xy_input = fl::vec2<uint16_t>(i16, i16);
            fl::vec2<uint16_t> xy = tx.transform(xy_input);
            INFO("i = " << i);
            REQUIRE_EQ(i, xy.x);
            REQUIRE_EQ(i, xy.y);
        }
    }

    SUBCASE("Check all bounds are in 255") {
        fl::Transform16 tx = fl::Transform16::ToBounds(255);
        uint32_t smallest = ~0;
        uint32_t largest = 0;
        for (uint16_t i = 0; i < 256; i++) {
            uint16_t i16 = fl::map8_to_16(i);
            fl::vec2<uint16_t> xy_input = fl::vec2<uint16_t>(i16, i16);
            fl::vec2<uint16_t> xy = tx.transform(xy_input);
            INFO("i = " << i);
            REQUIRE_LE(xy.x, 255);
            REQUIRE_LE(xy.y, 255);
            smallest = FL_MIN(smallest, xy.x);
            largest = FL_MAX(largest, xy.x);
        }

        REQUIRE_EQ(0, smallest);
        REQUIRE_EQ(255, largest);
    }
}

TEST_CASE("Transform16::ToBounds(min, max)") {
    SUBCASE("Check bounds at 128") {
        uint16_t low = 127;
        uint16_t high = 255 + 127;
        fl::vec2<uint16_t> min = fl::vec2<uint16_t>(low, low);
        fl::vec2<uint16_t> max = fl::vec2<uint16_t>(high, high);
        fl::Transform16 tx = fl::Transform16::ToBounds(min, max);
        auto t1 = tx.transform(fl::vec2<uint16_t>(0, 0));
        auto t2 = tx.transform(fl::vec2<uint16_t>(0xffff, 0xffff));
        REQUIRE_EQ(fl::vec2<uint16_t>(low, low), t1);
        REQUIRE_EQ(fl::vec2<uint16_t>(high, high), t2);
    }
}
