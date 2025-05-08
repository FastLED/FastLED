
// g++ --std=c++11 test.cpp

#include "test.h"

#include "test.h"
#include "fl/dbg.h"
#include "fl/bilinear_compression.h"



using namespace fl;

TEST_CASE("downscaleBilinear") {
    CRGB red = CRGB(255, 0, 0);
    CRGB black = CRGB(0, 0, 0);
    CRGB src[4] = {
        black, red
    };

    CRGB dst[1];
    downscaleBilinear(src, 2, 2, dst, 1, 1);
    INFO("Src: " << src);
    INFO("Dst: " << dst);
    CHECK(dst[0].r == 128);
    CHECK(dst[0].g == 0);
    CHECK(dst[0].b == 0);
}
