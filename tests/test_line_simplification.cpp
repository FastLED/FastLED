// g++ --std=c++11 test.cpp

#include "test.h"

#include "fl/line_simplification.h"
#include "fl/warn.h"


using namespace fl;

TEST_CASE("Test Line Simplification") {
    // default‐constructed bitset is empty
    LineSimplifier<float> ls;
    ls.setMinimumDistance(0.1f);
    fl::vector<point_xy<float>> points;
    points.push_back({0.0f, 0.0f});
    points.push_back({1.0f, 1.0f});
    points.push_back({2.0f, 2.0f});
    points.push_back({3.0f, 3.0f});
    points.push_back({4.0f, 4.0f});
    ls.simplifyInplace(&points);
    REQUIRE_EQ(2, points.size());  // Only 2 points on co-linear line should remain.
    REQUIRE_EQ(point_xy<float>(0.0f, 0.0f), points[0]);
    REQUIRE_EQ(point_xy<float>(4.0f, 4.0f), points[1]);
}

