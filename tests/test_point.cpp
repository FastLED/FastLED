// g++ --std=c++11 test.cpp

#include "test.h"
#include "fl/geometry.h"


using namespace fl;


#define REQUIRE_APPROX(a, b, tolerance) \
    do { \
        if (fabs((a - b)) > (tolerance)) { \
            std::cerr << "REQUIRE_APPROX failed: " << #a << " = " << (a) \
                      << ", " << #b << " = " << (b) \
                      << ", tolerance = " << (tolerance) << std::endl; \
            exit(1); \
        } \
    } while (0)


TEST_CASE("0 is 0 distance from diagonal line through the center") {
    line_xy<float> line(-100, -100, 100, 100);
    vec2<float> p(0, 0);
    vec2<float> projected;
    float dist = line.distance_to(p, &projected);
    REQUIRE_APPROX(projected.x, 0.0f, 0.001f);
    REQUIRE_APPROX(projected.y, 0.0f, 0.001f);
    REQUIRE_APPROX(dist, 0.0f, 0.001f);
}


TEST_CASE("point closest to line") {
    line_xy<float> line(-100, -100, 100, 100);

    vec2<float> p(50, 0);
    vec2<float> projected;

    float dist = line.distance_to(p, &projected);

    // The closest point should be (25, 25)
    REQUIRE_APPROX(projected.x, 25.0f, 0.001f);
    REQUIRE_APPROX(projected.y, 25.0f, 0.001f);

    // Distance should be sqrt((50-25)^2 + (0-25)^2) = sqrt(1250) ≈ 35.355
    REQUIRE_APPROX(dist, 35.355f, 0.001f);
}
