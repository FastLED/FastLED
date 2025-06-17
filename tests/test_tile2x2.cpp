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
    tile.setOrigin(1, 1);
    // Initialize the base tile with values
    tile.at(0, 0) = 1;
    tile.at(0, 1) = 2;
    tile.at(1, 0) = 3;
    tile.at(1, 1) = 4;

    // Create a wrapped version of the tile with wrap dimensions 2x2
    Tile2x2_u8_wrap wrap(tile, 2, 2);
    // Verify that coordinates are correctly wrapped
    // Each test checks both x and y coordinates of the wrapped position
    REQUIRE_EQ(wrap.at(0, 0).first.x, 1);
    REQUIRE_EQ(wrap.at(0, 0).first.y, 1);
    REQUIRE_EQ(wrap.at(0, 1).first.x, 1);
    REQUIRE_EQ(wrap.at(0, 1).first.y, 0);
    REQUIRE_EQ(wrap.at(1, 0).first.x, 0);
    REQUIRE_EQ(wrap.at(1, 0).first.y, 1);
    REQUIRE_EQ(wrap.at(1, 1).first.x, 0);
    REQUIRE_EQ(wrap.at(1, 1).first.y, 0);
}