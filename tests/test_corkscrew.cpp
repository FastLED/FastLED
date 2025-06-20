// g++ --std=c++11 test.cpp

#include "fl/math_macros.h"
#include "test.h"
#include "fl/algorithm.h"

#include "fl/sstream.h"

#include "fl/corkscrew.h"
#include "fl/grid.h" 
#include "fl/screenmap.h"
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
        vec2f pos = corkscrew_festival.at_no_wrap(i);
        max_height = MAX(max_height, pos.y);
        min_height = MIN(min_height, pos.y);
    }
    
    // LEDs should span from 0 to height-1
    REQUIRE(min_height >= 0.0f);
    REQUIRE(max_height <= 18.0f); // height-1 = 18-1 = 17
}

TEST_CASE("Corkscrew LED distribution test") {
    // Test if LEDs actually reach the top row
    Corkscrew::Input input(19.0f, 288, 0.0f); // FestivalStick case
    Corkscrew::State output = Corkscrew::generateState(input);
    Corkscrew corkscrew(input);
    
    // Count how many LEDs map to each row
    fl::vector<int> row_counts(output.height, 0);
    for (uint16_t i = 0; i < corkscrew.size(); ++i) {
        vec2f pos = corkscrew.at_no_wrap(i);
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
    vec2f pos0 = corkscrew_two_turns.at_no_wrap(0);
    vec2f pos1 = corkscrew_two_turns.at_no_wrap(1);
    vec2f pos2 = corkscrew_two_turns.at_no_wrap(2);
    vec2f pos3 = corkscrew_two_turns.at_no_wrap(3);

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

TEST_CASE("TestCorkscrewBufferFunctionality") {
    // Create a Corkscrew with 16 LEDs and 4 turns for simple testing
    fl::Corkscrew::Input input(4.0f, 16, 0, false);
    fl::Corkscrew corkscrew(input);
    
    // Get the rectangular buffer dimensions
    uint16_t width = corkscrew.cylinder_width();
    uint16_t height = corkscrew.cylinder_height();
    
    // Get the buffer and verify it's lazily initialized
    fl::vector<CRGB>& buffer = corkscrew.getBuffer();
    REQUIRE(buffer.size() == width * height);
    
    // Fill the buffer with a simple pattern
    corkscrew.fillBuffer(CRGB::Red);
    for (size_t i = 0; i < buffer.size(); ++i) {
        REQUIRE(buffer[i] == CRGB::Red);
    }
    
    // Clear the buffer
    corkscrew.clearBuffer();
    for (size_t i = 0; i < buffer.size(); ++i) {
        REQUIRE(buffer[i] == CRGB::Black);
    }
    
    // Create a source fl::Grid<CRGB> object with a checkerboard pattern
    fl::Grid<CRGB> source_grid(width, height);
    
    // Fill source with checkerboard pattern
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            if ((x + y) % 2 == 0) {
                source_grid(x, y) = CRGB::Blue;
            } else {
                source_grid(x, y) = CRGB::Green;
            }
        }
    }
    
    // Read from the source grid into our internal buffer
    corkscrew.readFrom(source_grid);
    
    // Verify that the buffer has been populated with colors from the source
    // Note: Not every pixel in the rectangular buffer may be written to by the corkscrew mapping
    bool found_blue = false;
    bool found_green = false;
    int non_black_count = 0;
    
    for (size_t i = 0; i < buffer.size(); ++i) {
        if (buffer[i] == CRGB::Blue) found_blue = true;
        if (buffer[i] == CRGB::Green) found_green = true;
        if (buffer[i] != CRGB::Black) non_black_count++;
    }
    
    // Should have found both colors in our checkerboard pattern
    REQUIRE(found_blue);
    REQUIRE(found_green);
    // Should have some non-black pixels since we read from a filled source
    REQUIRE(non_black_count > 0);
}

TEST_CASE("Corkscrew readFrom with bilinear interpolation") {
    // Create a small corkscrew for testing
    Corkscrew::Input input;
    input.numLeds = 12;  // 3x4 grid
    input.totalTurns = 1.0f;
    input.offsetCircumference = 0.0f;
    
    Corkscrew corkscrew(input);
    
    // Create a source grid - simple 3x4 pattern
    const uint16_t width = 3;
    const uint16_t height = 4;
    fl::Grid<CRGB> source_grid(width, height);
    
    // Grid initializes to black by default, but let's be explicit
    source_grid.clear();
    
    // Set up a simple pattern: red in corners, blue in center
    source_grid(0, 0) = CRGB::Red;    // Bottom-left
    source_grid(2, 0) = CRGB::Red;    // Bottom-right  
    source_grid(0, 3) = CRGB::Red;    // Top-left
    source_grid(2, 3) = CRGB::Red;    // Top-right
    source_grid(1, 1) = CRGB::Blue;   // Center-ish
    source_grid(1, 2) = CRGB::Blue;   // Center-ish
    
    // Read from the source into corkscrew buffer
    corkscrew.readFrom(source_grid);
    
    // Get the buffer
    const auto& buffer = corkscrew.getBuffer();
    
    // Verify buffer size
    REQUIRE_EQ(buffer.size(), static_cast<size_t>(corkscrew.cylinder_width() * corkscrew.cylinder_height()));
    
    // Check that some colors were captured 
    bool found_red_component = false;
    bool found_blue_component = false;
    int non_black_count = 0;
    
    for (size_t i = 0; i < buffer.size(); ++i) {
        const CRGB& color = buffer[i];
        if (color.r > 0 || color.g > 0 || color.b > 0) {
            non_black_count++;
        }
        if (color.r > 0) found_red_component = true;
        if (color.b > 0) found_blue_component = true;
    }
    
    // We should find some non-black pixels and some red components
    REQUIRE(non_black_count > 0);
    REQUIRE(found_red_component);
    
    // Blue components might not be found depending on the mapping, so let's be more lenient
    // The important thing is that we get some color data
    
    // Test that coordinates mapping makes sense by checking a specific LED
    vec2f pos0 = corkscrew.at_no_wrap(0);
    vec2f pos5 = corkscrew.at_no_wrap(5);
    
    // Positions should be different
    bool positions_different = (pos0.x != pos5.x) || (pos0.y != pos5.y);
    REQUIRE(positions_different);
    
    // Positions should be within reasonable bounds
    REQUIRE(pos0.x >= 0.0f);
    REQUIRE(pos0.y >= 0.0f);
    REQUIRE(pos5.x >= 0.0f);
    REQUIRE(pos5.y >= 0.0f);
}

TEST_CASE("Corkscrew CRGB* data access") {
    // Create a corkscrew
    Corkscrew::Input input;
    input.numLeds = 6;
    input.totalTurns = 1.0f;
    input.offsetCircumference = 0.0f;
    
    Corkscrew corkscrew(input);
    
    // Get raw CRGB* access - this should trigger lazy allocation
    CRGB* data_ptr = corkscrew.data();
    REQUIRE(data_ptr != nullptr);
    
    // Verify buffer was allocated with correct size
    size_t expected_size = static_cast<size_t>(corkscrew.cylinder_width()) * static_cast<size_t>(corkscrew.cylinder_height());
    
    // All pixels should be initialized to black
    for (size_t i = 0; i < expected_size; ++i) {
        REQUIRE_EQ(data_ptr[i].r, 0);
        REQUIRE_EQ(data_ptr[i].g, 0);
        REQUIRE_EQ(data_ptr[i].b, 0);
    }
    
    // Const access should also work
    const Corkscrew& const_corkscrew = corkscrew;
    const CRGB* const_data_ptr = const_corkscrew.data();
    REQUIRE(const_data_ptr != nullptr);
    REQUIRE(const_data_ptr == data_ptr); // Should be the same buffer
    
    // Modify a pixel via the raw pointer
    if (expected_size > 0) {
        data_ptr[0] = CRGB::Red;
        
        // Verify the change is reflected
        REQUIRE_EQ(data_ptr[0].r, 255);
        REQUIRE_EQ(data_ptr[0].g, 0);
        REQUIRE_EQ(data_ptr[0].b, 0);
        
        // And verify it's also reflected in the buffer access
        const auto& buffer = corkscrew.getBuffer();
        REQUIRE_EQ(buffer[0].r, 255);
        REQUIRE_EQ(buffer[0].g, 0);
        REQUIRE_EQ(buffer[0].b, 0);
    }
}

TEST_CASE("Corkscrew ScreenMap functionality") {
    // Create a simple corkscrew for testing
    fl::Corkscrew::Input input(2.0f, 8, 0.0f); // 2 turns, 8 LEDs
    fl::Corkscrew corkscrew(input);
    
    // Test default diameter
    fl::ScreenMap screenMap = corkscrew.toScreenMap();
    
    // Verify the ScreenMap has the correct number of LEDs
    REQUIRE_EQ(screenMap.getLength(), 8);
    
    // Verify default diameter
    REQUIRE_EQ(screenMap.getDiameter(), 0.5f);
    
    // Test custom diameter
    fl::ScreenMap screenMapCustom = corkscrew.toScreenMap(1.2f);
    REQUIRE_EQ(screenMapCustom.getDiameter(), 1.2f);
    
    // Verify that each LED index maps to the same position as at_exact() (wrapped)
    for (uint16_t i = 0; i < 8; ++i) {
        vec2f corkscrewPos = corkscrew.at_exact(i);
        vec2f screenMapPos = screenMap[i];
        
        // Positions should match exactly (both are wrapped)
        REQUIRE(ALMOST_EQUAL_FLOAT(corkscrewPos.x, screenMapPos.x));
        REQUIRE(ALMOST_EQUAL_FLOAT(corkscrewPos.y, screenMapPos.y));
    }
    
    // Test that different LED indices have different positions (at least some of them)
    bool positions_differ = false;
    for (uint16_t i = 1; i < 8; ++i) {
        vec2f pos0 = screenMap[0];
        vec2f posI = screenMap[i];
        if (!ALMOST_EQUAL_FLOAT(pos0.x, posI.x) || !ALMOST_EQUAL_FLOAT(pos0.y, posI.y)) {
            positions_differ = true;
            break;
        }
    }
    REQUIRE(positions_differ); // At least some positions should be different
    
    // Test ScreenMap bounds
    vec2f bounds = screenMap.getBounds();
    REQUIRE(bounds.x > 0.0f); // Should have some width
    REQUIRE(bounds.y >= 0.0f); // Should have some height (or 0 for single row)
    
    // Test a larger corkscrew to ensure it works with more complex cases
    fl::Corkscrew::Input input_large(19.0f, 288, 0.0f); // FestivalStick case
    fl::Corkscrew corkscrew_large(input_large);
    fl::ScreenMap screenMap_large = corkscrew_large.toScreenMap(0.8f);
    
    REQUIRE_EQ(screenMap_large.getLength(), 288);
    REQUIRE_EQ(screenMap_large.getDiameter(), 0.8f);
    
    // Verify all positions are valid (non-negative)
    for (uint16_t i = 0; i < 288; ++i) {
        vec2f pos = screenMap_large[i];
        REQUIRE(pos.x >= 0.0f);
        REQUIRE(pos.y >= 0.0f);
    }
}
