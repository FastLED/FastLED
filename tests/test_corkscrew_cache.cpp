#include "test.h"
#include "fl/corkscrew.h"
#include "fl/tile2x2.h"

using namespace fl;

TEST_CASE("Corkscrew caching functionality") {
    // Create a small corkscrew for testing
    Corkscrew::Input input(2.0f, 10); // 2 turns, 10 LEDs
    Corkscrew corkscrew(input);
    
    // Test that caching is enabled by default
    Tile2x2_u8_wrap tile1 = corkscrew.at_wrap(1.0f);
    Tile2x2_u8_wrap tile1_again = corkscrew.at_wrap(1.0f);
    
    // Values should be identical (from cache)
    for (int x = 0; x < 2; x++) {
        for (int y = 0; y < 2; y++) {
            auto data1 = tile1.at(x, y);
            auto data1_again = tile1_again.at(x, y);
            REQUIRE_EQ(data1.first.x, data1_again.first.x);
            REQUIRE_EQ(data1.first.y, data1_again.first.y);
            REQUIRE_EQ(data1.second, data1_again.second);
        }
    }
}

TEST_CASE("Corkscrew caching disable functionality") {
    // Create a small corkscrew for testing
    Corkscrew::Input input(2.0f, 10); // 2 turns, 10 LEDs
    Corkscrew corkscrew(input);
    
    // Get a tile with caching enabled
    Tile2x2_u8_wrap tile_cached = corkscrew.at_wrap(1.0f);
    
    // Disable caching
    corkscrew.setCachingEnabled(false);
    
    // Get the same tile with caching disabled
    Tile2x2_u8_wrap tile_uncached = corkscrew.at_wrap(1.0f);
    
    // Values should still be identical (same calculation)
    for (int x = 0; x < 2; x++) {
        for (int y = 0; y < 2; y++) {
            auto data_cached = tile_cached.at(x, y);
            auto data_uncached = tile_uncached.at(x, y);
            REQUIRE_EQ(data_cached.first.x, data_uncached.first.x);
            REQUIRE_EQ(data_cached.first.y, data_uncached.first.y);
            REQUIRE_EQ(data_cached.second, data_uncached.second);
        }
    }
    
    // Re-enable caching
    corkscrew.setCachingEnabled(true);
    
    // Get a tile again - should work with caching re-enabled
    Tile2x2_u8_wrap tile_recached = corkscrew.at_wrap(1.0f);
    
    // Values should still be identical
    for (int x = 0; x < 2; x++) {
        for (int y = 0; y < 2; y++) {
            auto data_cached = tile_cached.at(x, y);
            auto data_recached = tile_recached.at(x, y);
            REQUIRE_EQ(data_cached.first.x, data_recached.first.x);
            REQUIRE_EQ(data_cached.first.y, data_recached.first.y);
            REQUIRE_EQ(data_cached.second, data_recached.second);
        }
    }
}

TEST_CASE("Corkscrew caching with edge cases") {
    // Create a small corkscrew for testing
    Corkscrew::Input input(1.5f, 5); // 1.5 turns, 5 LEDs
    Corkscrew corkscrew(input);
    
    // Test caching with various float indices
    Tile2x2_u8_wrap tile0 = corkscrew.at_wrap(0.0f);
    Tile2x2_u8_wrap tile4 = corkscrew.at_wrap(4.0f);
    
    // Test that different indices produce different tiles
    bool tiles_different = false;
    for (int x = 0; x < 2 && !tiles_different; x++) {
        for (int y = 0; y < 2 && !tiles_different; y++) {
            auto data0 = tile0.at(x, y);
            auto data4 = tile4.at(x, y);
            if (data0.first.x != data4.first.x || 
                data0.first.y != data4.first.y || 
                data0.second != data4.second) {
                tiles_different = true;
            }
        }
    }
    REQUIRE(tiles_different); // Tiles at different positions should be different
    
    // Test that same index produces same tile (cache consistency)
    Tile2x2_u8_wrap tile0_again = corkscrew.at_wrap(0.0f);
    for (int x = 0; x < 2; x++) {
        for (int y = 0; y < 2; y++) {
            auto data0 = tile0.at(x, y);
            auto data0_again = tile0_again.at(x, y);
            REQUIRE_EQ(data0.first.x, data0_again.first.x);
            REQUIRE_EQ(data0.first.y, data0_again.first.y);
            REQUIRE_EQ(data0.second, data0_again.second);
        }
    }
}
