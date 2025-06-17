// Test file for Tile2x2 functionality
// g++ --std=c++11 test.cpp

#include "test.h"
#include "lib8tion/intmap.h"
#include "fl/transform.h"
#include "fl/vector.h"
#include "fl/tile2x2.h"
#include <string>

using namespace fl;

// Test basic Tile2x2_u8 functionality
// Verifies that a 2x2 tile can be created and values can be set/retrieved correctly
TEST_CASE("Tile2x2_u8") {
    Tile2x2_u8 tile;
    // Set the origin point of the tile
    tile.setOrigin(1, 1);
    // Set values in a 2x2 grid pattern
    tile.at(0, 0) = 1;
    tile.at(0, 1) = 2;
    tile.at(1, 0) = 3;
    tile.at(1, 1) = 4;

    // Verify all values are stored and retrieved correctly
    REQUIRE_EQ(tile.at(0, 0), 1);
    REQUIRE_EQ(tile.at(0, 1), 2);
    REQUIRE_EQ(tile.at(1, 0), 3);
    REQUIRE_EQ(tile.at(1, 1), 4);
}

// Test Tile2x2_u8_wrap functionality
// Verifies that a wrapped tile correctly maps coordinates to their wrapped positions
TEST_CASE("Tile2x2_u8_wrap") {
    Tile2x2_u8 tile;
    tile.setOrigin(4, 4);
    // Initialize the base tile with values
    tile.at(0, 0) = 1;
    tile.at(0, 1) = 2;
    tile.at(1, 0) = 3;
    tile.at(1, 1) = 4;

    // Create a wrapped version of the tile with wrap dimensions 2x2
    Tile2x2_u8_wrap wrap(tile, 2, 2);
    // Verify that coordinates are correctly wrapped
    // Each test checks both x and y coordinates of the wrapped position
    REQUIRE_EQ(wrap.at(0, 0).first.x, 0);
    REQUIRE_EQ(wrap.at(0, 0).first.y, 0);
    REQUIRE_EQ(wrap.at(0, 1).first.x, 0);
    REQUIRE_EQ(wrap.at(0, 1).first.y, 1);
    REQUIRE_EQ(wrap.at(1, 0).first.x, 1);
    REQUIRE_EQ(wrap.at(1, 0).first.y, 0);
    REQUIRE_EQ(wrap.at(1, 1).first.x, 1);
    REQUIRE_EQ(wrap.at(1, 1).first.y, 1);
}

// Test Tile2x2_u8_wrap wrap-around behavior
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

// Test Tile2x2_u8_wrap conversion with width and height
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

// Test Tile2x2_u8_wrap conversion with width only
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