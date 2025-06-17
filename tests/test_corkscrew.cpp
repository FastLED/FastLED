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
    Corkscrew corkscrew_festival(input_festival);
    for (uint16_t i = 0; i < corkscrew_festival.size(); ++i) {
        vec2f pos = corkscrew_festival.at_exact(i);
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
    Corkscrew corkscrew(input);
    
    // Count how many LEDs map to each row
    fl::vector<int> row_counts(output.height, 0);
    for (uint16_t i = 0; i < corkscrew.size(); ++i) {
        vec2f pos = corkscrew.at_exact(i);
        int row = static_cast<int>(pos.y);
        if (row >= 0 && row < output.height) {
            row_counts[row]++;
        }
    }
    
    // Check if top row (now row 17) has LEDs
    REQUIRE(row_counts[output.height - 1] > 0); // Top row should have LEDs
    REQUIRE(row_counts[0] > 0);  // Bottom row should have LEDs
}

TEST_CASE("Corkscrew two turns test") {
    // Test 2 turns with 2 LEDs per turn (4 LEDs total)
    Corkscrew::Input input_two_turns(2.0f, 4, 0.0f); // 2 turns, 4 LEDs, no offset
    Corkscrew::State output_two_turns = Corkscrew::generateState(input_two_turns);
    
    // Verify: 4 LEDs / 2 turns = 2 LEDs per turn
    REQUIRE_EQ(output_two_turns.width, 2);   // LEDs per turn
    REQUIRE_EQ(output_two_turns.height, 2);  // Number of turns
    
    // Verify grid size matches LED count
    REQUIRE_EQ(output_two_turns.width * output_two_turns.height, 4);
    
    // Test LED positioning across both turns
    Corkscrew corkscrew_two_turns(input_two_turns);
    REQUIRE_EQ(corkscrew_two_turns.size(), 4);
    
    // Check that LEDs are distributed across both rows
    fl::vector<int> row_counts(output_two_turns.height, 0);
    
    // Unrolled loop for 4 LEDs
    vec2f pos0 = corkscrew_two_turns.at_exact(0);
    vec2f pos1 = corkscrew_two_turns.at_exact(1);
    vec2f pos2 = corkscrew_two_turns.at_exact(2);
    vec2f pos3 = corkscrew_two_turns.at_exact(3);

    FL_WARN("pos0: " << pos0);
    FL_WARN("pos1: " << pos1);
    FL_WARN("pos2: " << pos2);
    FL_WARN("pos3: " << pos3);

    int row0 = static_cast<int>(pos0.y);
    if (row0 >= 0 && row0 < output_two_turns.height) {
        row_counts[row0]++;
    }
    
    
    int row1 = static_cast<int>(pos1.y);
    if (row1 >= 0 && row1 < output_two_turns.height) {
        row_counts[row1]++;
    }
    

    int row2 = static_cast<int>(pos2.y);
    if (row2 >= 0 && row2 < output_two_turns.height) {
        row_counts[row2]++;
    }
    

    int row3 = static_cast<int>(pos3.y);
    if (row3 >= 0 && row3 < output_two_turns.height) {
        row_counts[row3]++;
    }
    
    // Both rows should have LEDs
    REQUIRE(row_counts[0] > 0);  // First turn should have LEDs
    REQUIRE(row_counts[1] > 0);  // Second turn should have LEDs
}

TEST_CASE("Constexpr corkscrew dimension calculation") {
    // Test constexpr functions at compile time
    
    // FestivalStick case: 19 turns, 288 LEDs
    constexpr uint16_t festival_width = fl::calculateCorkscrewWidth(19.0f, 288);
    constexpr uint16_t festival_height = fl::calculateCorkscrewHeight(19.0f, 288);
    
    static_assert(festival_width == 16, "FestivalStick width should be 16");
    static_assert(festival_height == 18, "FestivalStick height should be 18");
    
    // Default case: 19 turns, 144 LEDs
    constexpr uint16_t default_width = fl::calculateCorkscrewWidth(19.0f, 144);
    constexpr uint16_t default_height = fl::calculateCorkscrewHeight(19.0f, 144);
    
    static_assert(default_width == 8, "Default width should be 8");
    static_assert(default_height == 18, "Default height should be 18");
    
    // Verify runtime and compile-time versions match
    fl::Corkscrew::Input runtime_input(19.0f, 288, 0.0f);
    fl::Corkscrew::State runtime_output = fl::Corkscrew::generateState(runtime_input);
    
    REQUIRE_EQ(festival_width, runtime_output.width);
    REQUIRE_EQ(festival_height, runtime_output.height);
    
    // Test simple perfect case: 100 LEDs, 10 turns = 10x10 grid
    constexpr uint16_t simple_width = fl::calculateCorkscrewWidth(10.0f, 100);
    constexpr uint16_t simple_height = fl::calculateCorkscrewHeight(10.0f, 100);
    
    static_assert(simple_width == 10, "Simple width should be 10");
    static_assert(simple_height == 10, "Simple height should be 10");
}
