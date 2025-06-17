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

TEST_CASE("Tile2x2_u8_wrap::Interpolate") {

    SUBCASE("Basic interpolation") {
        // Create test tiles by converting from regular Tile2x2_u8
        Tile2x2_u8 base_a, base_b;
        base_a.setOrigin(0, 0);
        base_b.setOrigin(0, 0);
        
        // Set up base tiles with different alpha values
        base_a.at(0, 0) = 100;
        base_a.at(0, 1) = 150;
        base_a.at(1, 0) = 200;
        base_a.at(1, 1) = 250;
        
        base_b.at(0, 0) = 200;
        base_b.at(0, 1) = 250;
        base_b.at(1, 0) = 50;
        base_b.at(1, 1) = 100;
        
        // Convert to wrapped tiles
        Tile2x2_u8_wrap tile_a(base_a, 10);
        Tile2x2_u8_wrap tile_b(base_b, 10);
        
        // Test interpolation at t=0.5
        auto result = Tile2x2_u8_wrap::Interpolate(tile_a, tile_b, 0.5f);
        
        REQUIRE(result.size() == 1);
        const auto& interpolated = result[0];
        
        // Check interpolated values (should be halfway between a and b)
        CHECK(interpolated.at(0, 0).second == 150); // (100 + 200) / 2
        CHECK(interpolated.at(0, 1).second == 200); // (150 + 250) / 2
        CHECK(interpolated.at(1, 0).second == 125); // (200 + 50) / 2
        CHECK(interpolated.at(1, 1).second == 175); // (250 + 100) / 2
        
        // Check that positions are preserved from tile_a
        CHECK(interpolated.at(0, 0).first.x == 0);
        CHECK(interpolated.at(0, 0).first.y == 0);
        CHECK(interpolated.at(1, 1).first.x == 1);
        CHECK(interpolated.at(1, 1).first.y == 1);
    }
    
    SUBCASE("Edge cases") {
        // Create simple test tiles
        Tile2x2_u8 base_a, base_b;
        base_a.setOrigin(0, 0);
        base_b.setOrigin(0, 0);
        base_a.at(0, 0) = 100;
        base_b.at(0, 0) = 200;
        
        Tile2x2_u8_wrap tile_a(base_a, 10);
        Tile2x2_u8_wrap tile_b(base_b, 10);
        
        // Test t=0 (should return tile_a)
        auto result_0 = Tile2x2_u8_wrap::Interpolate(tile_a, tile_b, 0.0f);
        REQUIRE(result_0.size() == 1);
        CHECK(result_0[0].at(0, 0).second == 100);
        
        // Test t=1 (should return tile_b)
        auto result_1 = Tile2x2_u8_wrap::Interpolate(tile_a, tile_b, 1.0f);
        REQUIRE(result_1.size() == 1);
        CHECK(result_1[0].at(0, 0).second == 200);
        
        // Test t<0 (should clamp to tile_a)
        auto result_neg = Tile2x2_u8_wrap::Interpolate(tile_a, tile_b, -0.5f);
        REQUIRE(result_neg.size() == 1);
        CHECK(result_neg[0].at(0, 0).second == 100);
        
        // Test t>1 (should clamp to tile_b)
        auto result_over = Tile2x2_u8_wrap::Interpolate(tile_a, tile_b, 1.5f);
        REQUIRE(result_over.size() == 1);
        CHECK(result_over[0].at(0, 0).second == 200);
    }
}