// g++ --std=c++11 test.cpp

#include "fl/math_macros.h"
#include "test.h"

#include "fl/corkscrew.h"

#define TWO_PI (PI * 2.0)

// Define an improved CHECK_CLOSE macro that provides better error messages
#define CHECK_CLOSE(a, b, epsilon)                                             \
    do {                                                                       \
        float _a = (a);                                                        \
        float _b = (b);                                                        \
        float _diff = fabsf(_a - _b);                                          \
        bool _result = _diff <= (epsilon);                                     \
        if (!_result) {                                                        \
            printf("CHECK_CLOSE failed: |%f - %f| = %f > %f\n", (float)_a,     \
                   (float)_b, _diff, (float)(epsilon));                        \
        }                                                                      \
        CHECK(_result);                                                        \
    } while (0)

using namespace fl;

TEST_CASE("Corkscrew generateMap") {
    Corkscrew::Input input;
    input.totalHeight = 10.0f;
    input.totalAngle = TWO_PI;
    input.offsetCircumference = 0.0f;

    Corkscrew::Output output;

    Corkscrew::generateMap(input, output);

    CHECK(output.width == 10);
    CHECK(output.height == 1);
    CHECK(output.mapping.size() == 10);

    // Check first pixel for correctness (basic integrity)
    CHECK(output.mapping[0].x >= 0.0f);
    CHECK(output.mapping[0].x <= 10.0f);
    CHECK(output.mapping[0].y >= 0.0f);
    CHECK(output.mapping[0].y <= 1.0f); // 1 vertical segment for 2π angle
}

TEST_CASE("Corkscrew generateMap with two turns") {
    Corkscrew::Input input;
    input.totalHeight = 10.0f;
    input.totalAngle = 2 * TWO_PI; // Two full turns
    input.offsetCircumference = 0.0f;

    Corkscrew::Output output = Corkscrew::generateMap(input);

    CHECK(output.width == 5);
    CHECK(output.height == 2);          // Two vertical segments for two turns
    CHECK(output.mapping.size() == 10); // 5 width * 2 height

    // Check first pixel for correctness (basic integrity)
    CHECK(output.mapping[0].x >= 0.0f);
    CHECK(output.mapping[0].x <= 5.0f);
    CHECK(output.mapping[0].y >= 0.0f);
    CHECK(output.mapping[0].y <= 2.0f); // 2 vertical segments for 4π angle
}

TEST_CASE("Corkscrew generateMap with LED count") {
    Corkscrew::Input input;
    input.totalHeight = 10.0f;
    input.totalAngle = TWO_PI; // One full turn
    input.offsetCircumference = 0.0f;

    input.numLeds = 20; // 20 LEDs around the corkscrew

    Corkscrew::Output output;
    Corkscrew::generateMap(input, output);

    // Check that the mapping has the correct size (based on numLeds)
    CHECK(output.mapping.size() == 20);

    // Check first LED position
    CHECK(output.mapping[0].x >= 0.0f);
    CHECK(output.mapping[0].x <= 10.0f);
    CHECK(output.mapping[0].y >= 0.0f);

    // Check last LED position
    CHECK(output.mapping[19].x >= 0.0f);
    CHECK(output.mapping[19].x <= 10.0f);
    CHECK(output.mapping[19].y <=
          1.0f); // Should be at the end of 1 vertical segment

    // Check that LEDs are distributed along the corkscrew
    CHECK(output.mapping[10].y > output.mapping[0].y);
    CHECK(output.mapping[19].y > output.mapping[10].y);
}

TEST_CASE("Corkscrew generateMap with 6 points over 2 turns") {
    Corkscrew::Input input;
    input.totalHeight = 10.0f;
    input.totalAngle = 2 * TWO_PI; // Two full turns
    input.offsetCircumference = 0.0f;

    input.numLeds = 6; // 6 LEDs around the corkscrew

    Corkscrew::Output output;
    Corkscrew::generateMap(input, output);

    // Check that the mapping has the correct size
    CHECK(output.mapping.size() == 6);

    // For 6 points over 2 turns with circumference = 5.0:
    // Point 0: (0.0, 0.0) - start of corkscrew
    // Point 1: (2.0, 0.4) - 1/5 through corkscrew
    // Point 2: (4.0, 0.8) - 2/5 through corkscrew
    // Point 3: (1.0, 1.2) - 3/5 through corkscrew (wrapped around)
    // Point 4: (3.0, 1.6) - 4/5 through corkscrew
    // Point 5: (0.0, 2.0) - end of corkscrew

    // Point 0 - start
    CHECK_CLOSE(output.mapping[0].x, 0.0f, 0.1f);
    CHECK_CLOSE(output.mapping[0].y, 0.0f, 0.1f);

    // Point 1 - 1/5 through
    CHECK_CLOSE(output.mapping[1].x, 2.0f, 0.1f);
    CHECK_CLOSE(output.mapping[1].y, 0.4f, 0.1f);

    // Point 2 - 2/5 through
    CHECK_CLOSE(output.mapping[2].x, 4.0f, 0.1f);
    CHECK_CLOSE(output.mapping[2].y, 0.8f, 0.1f);

    // Point 3 - 3/5 through (wraps around on x)
    CHECK_CLOSE(output.mapping[3].x, 1.0f, 0.1f);
    CHECK_CLOSE(output.mapping[3].y, 1.2f, 0.1f);

    // Point 4 - 4/5 through
    CHECK_CLOSE(output.mapping[4].x, 3.0f, 0.1f);
    CHECK_CLOSE(output.mapping[4].y, 1.6f, 0.1f);

    // Point 5 - end
    CHECK_CLOSE(output.mapping[5].x, 0.0f, 0.1f);
    CHECK_CLOSE(output.mapping[5].y, 2.0f, 0.1f);
}
