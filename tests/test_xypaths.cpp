
// g++ --std=c++11 test.cpp

#include "test.h"

#include "test.h"
#include "fl/xypaths.h"

using namespace fl;

TEST_CASE("LinePath") {
    LinePath path(0.0f, 0.0f, 1.0f, 1.0f);
    pair_xy_float xy = path.at(0.5f);
    CHECK(xy.x == 0.5f);
    CHECK(xy.y == 0.5f);

    xy = path.at(1.0f);
    CHECK(xy.x == 1.0f);
    CHECK(xy.y == 1.0f);

    xy = path.at(0.0f);
    CHECK(xy.x == 0.0f);
    CHECK(xy.y == 0.0f);
}


