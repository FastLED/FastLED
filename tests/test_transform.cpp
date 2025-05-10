
// g++ --std=c++11 test.cpp

#include "test.h"

#include "test.h"
#include "lib8tion/intmap.h"
#include "fl/transform.h"
#include "fl/vector.h"
#include "fl/unused.h"
#include <string>

using namespace fl;


TEST_CASE("Transform16::ToBounds(max_value)") {
    // Transform16 tx = Transform16::ToBounds(255);

    SUBCASE("Check bounds at 128") {
        // known bad at i == 128
        Transform16 tx = Transform16::ToBounds(255);
        uint16_t i16 = map8_to_16(128);
        vec2<uint16_t> xy_input = vec2<uint16_t>(i16, i16);
        vec2<uint16_t> xy = tx.transform(xy_input);
        INFO("i = " << 128);
        REQUIRE_EQ(128, xy.x);
        REQUIRE_EQ(128, xy.y);
    }

    SUBCASE("Check identity from 8 -> 16") {
        Transform16 tx = Transform16::ToBounds(255);
        for (uint16_t i = 0; i < 256; i++) {
            uint16_t i16 = map8_to_16(i);
            vec2<uint16_t> xy_input = vec2<uint16_t>(i16, i16);
            vec2<uint16_t> xy = tx.transform(xy_input);
            INFO("i = " << i);
            REQUIRE_EQ(i, xy.x);
            REQUIRE_EQ(i, xy.y);
        }
    }

    SUBCASE("Check all bounds are in 255") {
        Transform16 tx = Transform16::ToBounds(255);
        uint32_t smallest = ~0;
        uint32_t largest = 0;
        for (uint16_t i = 0; i < 256; i++) {
            uint16_t i16 = map8_to_16(i);
            vec2<uint16_t> xy_input = vec2<uint16_t>(i16, i16);
            vec2<uint16_t> xy = tx.transform(xy_input);
            INFO("i = " << i);
            REQUIRE_LE(xy.x, 255);
            REQUIRE_LE(xy.y, 255);
            smallest = MIN(smallest, xy.x);
            largest = MAX(largest, xy.x);
        }

        REQUIRE_EQ(0, smallest);
        REQUIRE_EQ(255, largest);
    }
}

TEST_CASE("Transform16::ToBounds(min, max)") {
    SUBCASE("Check bounds at 128") {
        uint16_t low = 127;
        uint16_t high = 255 + 127;
        vec2<uint16_t> min = vec2<uint16_t>(low, low);
        vec2<uint16_t> max = vec2<uint16_t>(high, high);
        Transform16 tx = Transform16::ToBounds(min, max);
        auto t1 = tx.transform(vec2<uint16_t>(0, 0));
        auto t2 = tx.transform(vec2<uint16_t>(0xffff, 0xffff));
        REQUIRE_EQ(vec2<uint16_t>(low, low), t1);
        REQUIRE_EQ(vec2<uint16_t>(high, high), t2);
    }
}
