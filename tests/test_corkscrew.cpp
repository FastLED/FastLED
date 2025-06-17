// g++ --std=c++11 test.cpp

#include "fl/math_macros.h"
#include "test.h"
#include "fl/algorithm.h"

#include "fl/sstream.h"

#include "fl/corkscrew.h"
#include "fl/grid.h"
#include "fl/tile2x2.h" // Ensure this header is included for Tile2x2_u8

#define NUM_LEDS 288

#define TWO_PI (PI * 2.0)

using namespace fl;

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
