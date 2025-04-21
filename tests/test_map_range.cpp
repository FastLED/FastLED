
// g++ --std=c++11 test.cpp

#include "test.h"

#include "test.h"
#include "fl/map_range.h"


using namespace fl;

TEST_CASE("map_range<uint8_t>") {
    
    CHECK_EQ(map_range<uint8_t>(0, 0, 255, 0, 255), 0);
    CHECK_EQ(map_range<uint8_t>(255, 0, 255, 0, 255), 255);
    CHECK_EQ(map_range<uint8_t>(128, 0, 255, 0, 255), 128);
    CHECK_EQ(map_range<uint8_t>(128, 0, 255, 0, 127), 63);
    // One past the max should roll over to to 128.
    CHECK_EQ(map_range<uint8_t>(128, 0, 127, 0, 127), 128);
}


TEST_CASE("map_range<uint16_t>") {
    CHECK_EQ(map_range<uint16_t>(0, 0, 65535, 0, 65535), 0);
    CHECK_EQ(map_range<uint16_t>(65535, 0, 65535, 0, 65535), 65535);
    CHECK_EQ(map_range<uint16_t>(32768, 0, 65535, 0, 65535), 32768);
    CHECK_EQ(map_range<uint16_t>(32768, 0, 65535, 0, 32767), 16383);
    CHECK_EQ(map_range<uint16_t>(32768, 0, 32767, 0, 32767), 32768);
}


TEST_CASE("map_range<float>") {
    CHECK_EQ(map_range<float>(0.0f, 0.0f, 1.0f, 0.0f, 1.0f), 0.0f);
    CHECK_EQ(map_range<float>(1.0f, 0.0f, 1.0f, 0.0f, 1.0f), 1.0f);
    CHECK_EQ(map_range<float>(0.5f, 0.0f, 1.0f, 0.0f, 1.0f), 0.5f);
    CHECK_EQ(map_range<float>(0.5f, 0.0f, 1.0f, 10.0f, 20.0f), 15.0f);
    CHECK_EQ(map_range<float>(2.5f, -1.5f, 2.5f, -10.5f, -20.5f), -20.5f);
}

TEST_CASE("map_range<float, point_xy<float>") {
    float min = 0.0f;
    float max = 1.0f;
    point_xy<float> in_min(0.0f, 0.0f);
    point_xy<float> out_max(1.0f, 1.0f);

    point_xy<float> out = map_range(.5f, min, max, in_min, out_max);
    CHECK_EQ(out.x, 0.5f);
    CHECK_EQ(out.y, 0.5f);

    // Now try negative number
    out = map_range(-1.f, min, max, in_min, out_max);
    CHECK_EQ(out.x, -1.f);
    CHECK_EQ(out.y, -1.f);    
}