// g++ --std=c++11 test.cpp

#include "fl/math_macros.h"
#include "test.h"
#include "fl/algorithm.h"

#include "fl/sstream.h"

#include "fl/corkscrew.h"
#include "fl/grid.h"

#define NUM_LEDS 288

#define TWO_PI (PI * 2.0)

using namespace fl;

#if 0
TEST_CASE("Corkscrew generateState") {
    Corkscrew::Input input;
    input.totalHeight = 10.0f;
    input.totalTurns = 1;
    input.offsetCircumference = 0.0f;
    input.numLeds = 10;

    Corkscrew::State output = Corkscrew::generateState(input);

    CHECK_EQ(output.width, 10);
    CHECK_EQ(output.height, 11);         // One vertical segment for one turn
    CHECK_EQ(output.mapping.size(), 10); // 10 LEDs around the corkscrew

    CHECK_GE(output.mapping[0].x, 0.0f);
    CHECK_LE(output.mapping[0].x, 10.0f);
    CHECK_GE(output.mapping[0].y, 0.0f);
    CHECK_LE(output.mapping[0].y, 1.0f); // 1 vertical segment for 2π angle
}

TEST_CASE("Vertical corkscrew mapping") {
    // Tests that a corkscrew with only three leds aligned vertically
    // will produce the expected output.
    const int kCorkscrewTotalHeight = 1; // cm
    const int kCorkscrewTurns = 2; // Default to 19 turns
    const int kNumLeds = 3;


    vec2f expected_outputs[] = {
        {0.0f, 0.0f}, // First LED at the bottom
        {0.0f, 0.5f}, // Second LED in the middle
        {0.0f, 1.0f}  // Third LED at the top
    };

    Corkscrew::Input input;
    input.totalHeight = kCorkscrewTotalHeight;
    input.totalTurns = kCorkscrewTurns; // Default to 19 turns
    input.offsetCircumference = 0.0f;
    input.numLeds = kNumLeds;
    // Generate the corkscrew map
    Corkscrew corkscrew(input);

    REQUIRE_EQ(1, corkscrew.cylinder_width());
    REQUIRE_EQ(2, corkscrew.cylinder_height()); // 3 vertical segments for 2π angle

    Corkscrew::iterator it = corkscrew.begin();
    Corkscrew::iterator end = corkscrew.end();

    size_t count = end - it;

    const bool is_equal = fl::equal(it, end, expected_outputs);
    REQUIRE(is_equal);
}

TEST_CASE("Corkscrew generateState with two turns") {
    Corkscrew::Input input;
    input.totalHeight = 10.0f;
    input.totalTurns = 2; // Two full turns
    input.numLeds = 10;   // 10 LEDs around the corkscrew
    input.offsetCircumference = 0.0f;

    Corkscrew::State output = Corkscrew::generateState(input);

    CHECK_EQ(output.width, 6);
    CHECK_EQ(output.height, 11);         // Two vertical segments for two turns
    CHECK_EQ(output.mapping.size(), 10); // 5 width * 2 height

    // Check first pixel for correctness (basic integrity)
    CHECK_GE(output.mapping[0].x, 0.0f);
    CHECK_LE(output.mapping[0].x, 5.0f);
    CHECK_GE(output.mapping[0].y, 0.0f);
    CHECK_LE(output.mapping[0].y, 2.0f); // 2 vertical segments for 4π angle
}

TEST_CASE("Corkscrew generated map as a circle") {
    Corkscrew::Input input;
    input.totalHeight = 0.f;
    input.totalTurns = 1; // One full turn
    input.offsetCircumference = 0.0f;
    input.numLeds = 3; // 10 LEDs around the corkscrew

    // Corkscrew::State output = Corkscrew::generateState(input);
    Corkscrew corkscrew(input);
    vec2f first =
        corkscrew.at(0); // Access the first element to trigger generation
    vec2f second =
        corkscrew.at(1); // Access the second element to trigger generation
    vec2f third =
        corkscrew.at(2); // Access the third element to trigger generation

    vec2f expected_outputs[] = {
        {0.0f, 0.0f}, // First LED at the bottom
        {0.5f, 0.0f}, // Second LED in the middle
        {0.0f, 0.0f}  // Third LED at the top
    };

    auto begin = corkscrew.begin();
    auto end = corkscrew.end();
    size_t count = end - begin;
    REQUIRE_EQ(count, 3);
    const bool is_equal = fl::equal(begin, end, expected_outputs);
    REQUIRE(is_equal);
}

TEST_CASE("Corkscrew circumference test") {
    Corkscrew::Input input;
    // Use defaults: totalHeight = 100, totalAngle = 19 * 2 * PI
    input.totalHeight = 23.25f; // Total height of the corkscrew in centimeters
    input.totalTurns = 19.0f;   // Default to 19 turns
    input.offsetCircumference = 0.0f; // No offset
    input.numLeds = 288; // Default to dense 144 LEDs times two strips

    Corkscrew::State output = Corkscrew::generateState(input);

    // Basic sanity checks
    CHECK_EQ(output.width, 3);
    CHECK_EQ(output.height, 25);
    CHECK_EQ(output.mapping.size(), 288);

    // Check that circumference matches calculated value
    // float expectedCircumference = 100.0f / 19.0f;
    // CHECK_CLOSE(output.circumference, expectedCircumference, 0.01f);
}

TEST_CASE("Corkscrew circumference test") {
    Corkscrew::Input input;
    input.totalHeight = 1.0f;
    input.totalTurns = 1.0f;  
    input.offsetCircumference = 0.0f; // No offset
    input.numLeds = 3; // Default to dense 144 LEDs times two strips

    Corkscrew::State output = Corkscrew::generateState(input);

    // Basic sanity checks
    REQUIRE_EQ(2, output.width);
    REQUIRE_EQ(2, output.height);
    REQUIRE_EQ(3, output.mapping.size());

    fl::vector<vec2f> expected_value;
    expected_value.push_back(vec2f(0.0f, 0.0f)); // First LED at the bottom
    expected_value.push_back(vec2f(0.5f, 0.5f)); // Second LED in the middle
    expected_value.push_back(vec2f(0.0f, 1.0f)); // Third LED at the top
    const bool is_same = fl::equal_container(output.mapping, expected_value);
    REQUIRE(is_same);
}

#endif   // 0

TEST_CASE("Corkscrew Circle10 test") {
    Corkscrew::Input input;
    input.totalLength = 10.0f; // Total length of the corkscrew in centimeters
    input.totalHeight = 0.0f;
    input.totalTurns = 1.0f;  
    input.offsetCircumference = 0.0f; // No offset
    input.numLeds = 10; // Default to dense 144 LEDs times two strips
    Corkscrew::State output = Corkscrew::generateState(input);
    fl::vector<vec2f> expected_values;
    expected_values.push_back(vec2f(0.0f, 0.0f)); // First LED at the bottom
    expected_values.push_back(vec2f(1.0f, 0.0f)); // Second LED in the middle
    expected_values.push_back(vec2f(2.0f, 0.0f)); // Third LED at the top
    expected_values.push_back(vec2f(3.0f, 0.0f)); // Fourth LED at the top
    expected_values.push_back(vec2f(4.0f, 0.0f)); // Fifth LED at the top
    expected_values.push_back(vec2f(5.0f, 0.0f)); // Sixth LED at the top
    expected_values.push_back(vec2f(6.0f, 0.0f)); // Seventh LED at the top
    expected_values.push_back(vec2f(7.0f, 0.0f)); // Eighth LED at the top
    expected_values.push_back(vec2f(8.0f, 0.0f)); // Ninth LED at the top
    expected_values.push_back(vec2f(9.0f, 0.0f)); // Tenth LED at the top

    REQUIRE_EQ(output.width, 10);
    REQUIRE_EQ(output.height, 1);

}
