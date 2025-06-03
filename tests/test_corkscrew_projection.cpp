// g++ --std=c++11 test.cpp

#include "test.h"

#include "fl/corkscrew.h"

#define TWO_PI (PI * 2.0)

using namespace fl;



TEST_CASE("Corkscrew generateMap") {
    Corkscrew::Input input;
    input.totalCircumference = 10.0f;
    input.totalAngle = TWO_PI;
    input.offsetCircumference = 0.0f;
    input.compact = false;

    Corkscrew::Output output;

    Corkscrew::generateMap(input, output);

    CHECK(output.width == 10);
    CHECK(output.height == 1);
    CHECK(output.mapping.size() == 10);

    // Check first pixel for correctness (basic integrity)
    CHECK(output.mapping[0].x >= 0.0f);
    CHECK(output.mapping[0].x <= input.totalCircumference);
    CHECK(output.mapping[0].y >= 0.0f);
    CHECK(output.mapping[0].y <= 1.0f); // 1 vertical segment for 2π angle
}

TEST_CASE("Corkscrew generateMap with two turns") {
    Corkscrew::Input input;
    input.totalCircumference = 10.0f;
    input.totalAngle = 2 * TWO_PI; // Two full turns
    input.offsetCircumference = 0.0f;
    input.compact = false;

    Corkscrew::Output output = Corkscrew::generateMap(input);

    CHECK(output.width == 10);
    CHECK(output.height == 2);          // Two vertical segments for two turns
    CHECK(output.mapping.size() == 20); // 10 width * 2 height

    // Check first pixel for correctness (basic integrity)
    CHECK(output.mapping[0].x >= 0.0f);
    CHECK(output.mapping[0].x <= input.totalCircumference);
    CHECK(output.mapping[0].y >= 0.0f);
    CHECK(output.mapping[0].y <= 2.0f); // 2 vertical segments for 4π angle
}
