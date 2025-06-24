
// g++ --std=c++11 test.cpp

#include "test.h"

#include "test.h"
#include "fl/splat.h"

#include "fl/namespace.h"

using namespace fl;

TEST_CASE("splat simple test") {
    // Define a simple input coordinate
    vec2f input(0, 0);

    // Call the splat function
    Tile2x2_u8 result = splat(input);

    REQUIRE(result.bounds().mMin.x == 0);
    REQUIRE(result.bounds().mMin.y == 0);
    REQUIRE(result.bounds().mMax.x == 2);
    REQUIRE(result.bounds().mMax.y == 2);


    // Verify the output
    REQUIRE_EQ(result.lower_left(), 255);  // Expected intensity for lower-left
    REQUIRE_EQ(result.lower_right(), 0); // Expected intensity for lower-right
    REQUIRE_EQ(result.upper_left(), 0);  // Expected intensity for upper-left
    REQUIRE_EQ(result.upper_right(), 0); // Expected intensity for upper-right
}

TEST_CASE("splat test for input (0.0, 0.5)") {
    // Define the input coordinate
    vec2f input(0.0f, 0.5f);

    // Call the splat function
    Tile2x2_u8 result = splat(input);

    // Verify the bounds of the tile
    REQUIRE(result.bounds().mMin.x == 0);
    REQUIRE(result.bounds().mMin.y == 0);
    REQUIRE(result.bounds().mMax.x == 2);
    REQUIRE(result.bounds().mMax.y == 2);

    // Verify the output intensities
    REQUIRE_EQ(result.lower_left(), 128);  // Expected intensity for lower-left
    REQUIRE_EQ(result.lower_right(), 0);   // Expected intensity for lower-right
    REQUIRE_EQ(result.upper_left(), 128);  // Expected intensity for upper-left
    REQUIRE_EQ(result.upper_right(), 0);   // Expected intensity for upper-right
}

TEST_CASE("splat test for input (0.0, 0.99)") {
    // Define the input coordinate
    vec2f input(0.0f, 0.99f);

    // Call the splat function
    Tile2x2_u8 result = splat(input);

    // Verify the bounds of the tile
    REQUIRE(result.bounds().mMin.x == 0);
    REQUIRE(result.bounds().mMin.y == 0);
    REQUIRE(result.bounds().mMax.x == 2);
    REQUIRE(result.bounds().mMax.y == 2);

    // Verify the output intensities
    REQUIRE_EQ(result.lower_left(), 3);    // Expected intensity for lower-left
    REQUIRE_EQ(result.lower_right(), 0);   // Expected intensity for lower-right
    REQUIRE_EQ(result.upper_left(), 252);  // Expected intensity for upper-left
    REQUIRE_EQ(result.upper_right(), 0);   // Expected intensity for upper-right
}

TEST_CASE("splat test for input (0.0, 1.0)") {
    // Define the input coordinate
    vec2f input(0.0f, 1.0f);

    // Call the splat function
    Tile2x2_u8 result = splat(input);

    // Verify the bounds of the tile
    REQUIRE(result.bounds().mMin.x == 0);
    REQUIRE(result.bounds().mMin.y == 1);
    REQUIRE(result.bounds().mMax.x == 2);
    REQUIRE(result.bounds().mMax.y == 3);

    // Verify the output intensities
    REQUIRE_EQ(result.lower_left(), 255);    // Expected intensity for lower-left
    REQUIRE_EQ(result.lower_right(), 0);   // Expected intensity for lower-right
    REQUIRE_EQ(result.upper_left(), 0);   // Expected intensity for upper-left
    REQUIRE_EQ(result.upper_right(), 0);   // Expected intensity for upper-right
}

TEST_CASE("splat test for input (0.5, 0.0)") {
    // Define the input coordinate
    vec2f input(0.5f, 0.0f);

    // Call the splat function
    Tile2x2_u8 result = splat(input);

    // Verify the bounds of the tile
    REQUIRE(result.bounds().mMin.x == 0);
    REQUIRE(result.bounds().mMin.y == 0);
    REQUIRE(result.bounds().mMax.x == 2);
    REQUIRE(result.bounds().mMax.y == 2);

    // Verify the output intensities
    REQUIRE_EQ(result.lower_left(), 128);  // Expected intensity for lower-left
    REQUIRE_EQ(result.lower_right(), 128); // Expected intensity for lower-right
    REQUIRE_EQ(result.upper_left(), 0);    // Expected intensity for upper-left
    REQUIRE_EQ(result.upper_right(), 0);   // Expected intensity for upper-right
}

TEST_CASE("splat test for input (0.99, 0.0)") {
    // Define the input coordinate
    vec2f input(0.99f, 0.0f);

    // Call the splat function
    Tile2x2_u8 result = splat(input);

    // Verify the bounds of the tile
    REQUIRE(result.bounds().mMin.x == 0);
    REQUIRE(result.bounds().mMin.y == 0);
    REQUIRE(result.bounds().mMax.x == 2);
    REQUIRE(result.bounds().mMax.y == 2);

    // Verify the output intensities
    REQUIRE_EQ(result.lower_left(), 3);    // Expected intensity for lower-left
    REQUIRE_EQ(result.lower_right(), 252); // Expected intensity for lower-right
    REQUIRE_EQ(result.upper_left(), 0);    // Expected intensity for upper-left
    REQUIRE_EQ(result.upper_right(), 0);   // Expected intensity for upper-right
}

TEST_CASE("splat test for input (1.0, 0.0)") {
    // Define the input coordinate
    vec2f input(1.0f, 0.0f);

    // Call the splat function
    Tile2x2_u8 result = splat(input);

    // Verify the bounds of the tile
    REQUIRE(result.bounds().mMin.x == 1);
    REQUIRE(result.bounds().mMin.y == 0);
    REQUIRE(result.bounds().mMax.x == 3);
    REQUIRE(result.bounds().mMax.y == 2);

    // Verify the output intensities
    REQUIRE_EQ(result.lower_left(), 255);    // Expected intensity for lower-left
    REQUIRE_EQ(result.lower_right(), 0); // Expected intensity for lower-right
    REQUIRE_EQ(result.upper_left(), 0);    // Expected intensity for upper-left
    REQUIRE_EQ(result.upper_right(), 0);   // Expected intensity for upper-right
}
