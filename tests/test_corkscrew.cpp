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

TEST_CASE("Tile2x2_u8_wrap wrap-around test with width and height") {
    // Initialize a Tile2x2_u8 with known values and set origin beyond boundaries
    Tile2x2_u8 originalTile;
    originalTile.setOrigin(3, 3); // Set the origin beyond the width and height
    originalTile.at(0, 0) = 1;
    originalTile.at(0, 1) = 2;
    originalTile.at(1, 0) = 3;
    originalTile.at(1, 1) = 4;

    // Convert to Tile2x2_u8_wrap with given width and height
    uint16_t width = 2;
    uint16_t height = 2;
    Tile2x2_u8_wrap cycTile(originalTile, width, height);

    // Verify that the conversion wraps around correctly
    REQUIRE_EQ(cycTile.at(0, 0).first.x, 1); // Wraps around to (1, 1)
    REQUIRE_EQ(cycTile.at(0, 0).first.y, 1);
    REQUIRE_EQ(cycTile.at(0, 1).first.x, 1); // Wraps around to (1, 0)
    REQUIRE_EQ(cycTile.at(0, 1).first.y, 0);
    REQUIRE_EQ(cycTile.at(1, 0).first.x, 0); // Wraps around to (0, 1)
    REQUIRE_EQ(cycTile.at(1, 0).first.y, 1);
    REQUIRE_EQ(cycTile.at(1, 1).first.x, 0); // Wraps around to (0, 0)
    REQUIRE_EQ(cycTile.at(1, 1).first.y, 0);

    // Verify that the values are correct
    REQUIRE_EQ(cycTile.at(0, 0).second, 1);
    REQUIRE_EQ(cycTile.at(0, 1).second, 2);
    REQUIRE_EQ(cycTile.at(1, 0).second, 3);
    REQUIRE_EQ(cycTile.at(1, 1).second, 4);
}

TEST_CASE("Tile2x2_u8_wrap conversion with width and height") {
    // Initialize a Tile2x2_u8 with known values
    Tile2x2_u8 originalTile;
    originalTile.setOrigin(0, 0); // Set the origin to (0, 0)
    originalTile.at(0, 0) = 1;
    originalTile.at(0, 1) = 2;
    originalTile.at(1, 0) = 3;
    originalTile.at(1, 1) = 4;

    // Convert to Tile2x2_u8_wrap with given width and height
    uint16_t width = 2;
    uint16_t height = 2;
    Tile2x2_u8_wrap cycTile(originalTile, width, height);

    // Verify that the conversion is correct
    REQUIRE_EQ(cycTile.at(0, 0).second, 1);
    REQUIRE_EQ(cycTile.at(0, 1).second, 2);
    REQUIRE_EQ(cycTile.at(1, 0).second, 3);
    REQUIRE_EQ(cycTile.at(1, 1).second, 4);
}

TEST_CASE("Tile2x2_u8_wrap conversion test") {
    // Initialize a Tile2x2_u8 with known values and a specific origin
    Tile2x2_u8 originalTile;
    originalTile.setOrigin(50, 50); // Set the origin to (50, 50)
    originalTile.at(0, 0) = 1; // Initialize the missing element
    originalTile.at(0, 1) = 2;
    originalTile.at(1, 0) = 3;
    originalTile.at(1, 1) = 4;

    // Convert to Tile2x2_u8_wrap with a given width
    uint16_t width = 10;
    Tile2x2_u8_wrap cycTile(originalTile, width);

    // Verify that the conversion is correct
    REQUIRE_EQ(cycTile.at(0, 0).second, 1);
    REQUIRE_EQ(cycTile.at(0, 1).second, 2);
    REQUIRE_EQ(cycTile.at(1, 0).second, 3);
    REQUIRE_EQ(cycTile.at(1, 1).second, 4);

    // Verify wrap-around behavior on the x-axis
    REQUIRE_EQ(cycTile.at(2, 2).second, 1); // Wraps around to (0, 0)
    REQUIRE_EQ(cycTile.at(2, 3).second, 2); // Wraps around to (0, 1)
    REQUIRE_EQ(cycTile.at(3, 2).second, 3); // Wraps around to (1, 0)
    REQUIRE_EQ(cycTile.at(3, 3).second, 4); // Wraps around to (1, 1)
}
