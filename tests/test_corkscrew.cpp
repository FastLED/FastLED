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
    // Test the auto-calculating constructor
    Corkscrew::Input input_auto(1.0f, 10, 0.0f); // 1 turn, 10 LEDs, no offset
    Corkscrew::State output_auto = Corkscrew::generateState(input_auto);
    
    // Verify auto-calculated dimensions
    REQUIRE_EQ(output_auto.width, 10);  // ceil(10 LEDs / 1 turn) = 10
    REQUIRE_EQ(output_auto.height, 1);  // ceil(1 turn) = 1
    
    // Test your specific example: 20 LEDs with 2 turns (10 LEDs per turn)
    Corkscrew::Input input_example(2.0f, 20, 0.0f); // 2 turns, 20 LEDs, no offset
    Corkscrew::State output_example = Corkscrew::generateState(input_example);
    
    // Verify: 20 LEDs / 2 turns = 10 LEDs per turn
    REQUIRE_EQ(output_example.width, 10);  // LEDs per turn
    REQUIRE_EQ(output_example.height, 2);  // Number of turns
    
    // Test default constructor
    Corkscrew::Input input_default;
    Corkscrew::State output_default = Corkscrew::generateState(input_default);
    
    // Verify defaults: 144 LEDs / 19 turns ≈ 7.58 → ceil = 8
    REQUIRE_EQ(output_default.width, 8);   // ceil(144/19) = ceil(7.58) = 8
    REQUIRE_EQ(output_default.height, 18); // Optimized: ceil(144/8) = ceil(18) = 18 (perfect match!)
    
    // Test FestivalStick case: 19 turns, 288 LEDs 
    Corkscrew::Input input_festival(19.0f, 288, 0.0f);
    Corkscrew::State output_festival = Corkscrew::generateState(input_festival);
    
    // Debug the height calculation - should now optimize to prevent excess pixels
    REQUIRE_EQ(output_festival.width, 16);  // ceil(288/19) = ceil(15.16) = 16  
    REQUIRE_EQ(output_festival.height, 18); // ceil(288/16) = ceil(18) = 18 (optimized!)
    
    // Verify grid size matches LED count
    REQUIRE_EQ(output_festival.width * output_festival.height, 288);
    
    // Check LED distribution - find max height position actually used
    float max_height = 0.0f;
    float min_height = 999.0f;
    for (const auto& pos : output_festival.mapping) {
        max_height = MAX(max_height, pos.y);
        min_height = MIN(min_height, pos.y);
    }
    
    // LEDs should span from 0 to height-1
    REQUIRE(min_height >= 0.0f);
    REQUIRE(max_height <= 17.0f); // height-1 = 18-1 = 17
}

TEST_CASE("Corkscrew LED distribution test") {
    // Test if LEDs actually reach the top row
    Corkscrew::Input input(19.0f, 288, 0.0f); // FestivalStick case
    Corkscrew::State output = Corkscrew::generateState(input);
    
    // Count how many LEDs map to each row
    fl::vector<int> row_counts(output.height, 0);
    for (const auto& pos : output.mapping) {
        int row = static_cast<int>(pos.y);
        if (row >= 0 && row < output.height) {
            row_counts[row]++;
        }
    }
    
    // Check if top row (now row 17) has LEDs
    REQUIRE(row_counts[output.height - 1] > 0); // Top row should have LEDs
    REQUIRE(row_counts[0] > 0);  // Bottom row should have LEDs
}
