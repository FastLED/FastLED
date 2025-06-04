// g++ --std=c++11 test.cpp

#include "fl/math_macros.h"
#include "test.h"

#include "fl/corkscrew.h"


#define NUM_LEDS 288
#define CORKSCREW_TOTAL_HEIGHT                                                 \
    23.25f // Total height of the corkscrew in centimeters for 144 densly
           // wrapped up over 19 turns

#define CORKSCREW_WIDTH 16
#define CORKSCREW_HEIGHT 19

#define CORKSCREW_TURNS 19.f // Default to 19 turns

#define TWO_PI (PI * 2.0)

// Define an improved CHECK_CLOSE macro that provides better error messages
#define CHECK_CLOSE(a, b, epsilon)                                             \
    do {                                                                       \
        float _a = (a);                                                        \
        float _b = (b);                                                        \
        float _diff = fabsf(_a - _b);                                          \
        bool _result = _diff <= (epsilon);                                     \
        if (!_result) {                                                        \
            printf("CHECK_CLOSE failed: |%f - %f| = %f > %f\n", (float)_a,     \
                   (float)_b, _diff, (float)(epsilon));                        \
        }                                                                      \
        CHECK(_result);                                                        \
    } while (0)

using namespace fl;

TEST_CASE("Corkscrew generateMap") {
    Corkscrew::Input input;
    input.totalHeight = 10.0f;
    input.totalAngle = TWO_PI;
    input.offsetCircumference = 0.0f;
    input.numLeds = 10;

    Corkscrew::Output output = Corkscrew::generateMap(input);

    CHECK_EQ(output.width, 10);
    CHECK_EQ(output.height, 1);          // One vertical segment for one turn
    CHECK_EQ(output.mapping.size(), 10); // 10 LEDs around the corkscrew

    CHECK_GE(output.mapping[0].x, 0.0f);
    CHECK_LE(output.mapping[0].x, 10.0f);
    CHECK_GE(output.mapping[0].y, 0.0f);
    CHECK_LE(output.mapping[0].y, 1.0f); // 1 vertical segment for 2π angle
}

TEST_CASE("Corkscrew to Frame Buffer Mapping") {
    // Define the corkscrew input parameters
    Corkscrew::Input input;
    input.totalHeight = CORKSCREW_TOTAL_HEIGHT;
    input.totalAngle = CORKSCREW_TURNS * 2 * PI; // Default to 19 turns
    input.offsetCircumference = 0.0f;
    input.numLeds = NUM_LEDS;

    // Generate the corkscrew map

    Corkscrew corkscrew(input);

    // Create a frame buffer
    LedsXY<CORKSCREW_WIDTH, CORKSCREW_HEIGHT> frameBuffer;

    // Simulate the mapping logic from the loop function
    for (int i = 0; i < NUM_LEDS; ++i) {
        // Get the position in the frame buffer
        // vec2<int16_t> pos(static_cast<int16_t>(output.mapping[i].x), static_cast<int16_t>(output.mapping[i].y));
        vec2<int16_t> pos = corkscrew.at(i); // Get the position for LED i

        // Verify that the position is within the frame buffer bounds
        CHECK_GE(pos.x, 0);
        CHECK_LE(pos.x, CORKSCREW_WIDTH);
        CHECK_GE(pos.y, 0);
        CHECK_LE(pos.y, CORKSCREW_HEIGHT);

        // Optionally, verify the color mapping logic if needed
        // For example, check that the color is correctly set in the frame buffer
        // CRGB c = frameBuffer.at(pos.x, pos.y);
        // CHECK_EQ(c, expectedColor); // Define expectedColor based on your logic
    }
}

TEST_CASE("Corkscrew generateMap with two turns") {
    Corkscrew::Input input;
    input.totalHeight = 10.0f;
    input.totalAngle = 2 * TWO_PI; // Two full turns
    input.numLeds = 10;            // 10 LEDs around the corkscrew
    input.offsetCircumference = 0.0f;

    Corkscrew::Output output = Corkscrew::generateMap(input);

    CHECK_EQ(output.width, 5);
    CHECK_EQ(output.height, 2);          // Two vertical segments for two turns
    CHECK_EQ(output.mapping.size(), 10); // 5 width * 2 height

    // Check first pixel for correctness (basic integrity)
    CHECK_GE(output.mapping[0].x, 0.0f);
    CHECK_LE(output.mapping[0].x, 5.0f);
    CHECK_GE(output.mapping[0].y, 0.0f);
    CHECK_LE(output.mapping[0].y, 2.0f); // 2 vertical segments for 4π angle
}

TEST_CASE("Corkscrew circumference test") {
    Corkscrew::Input input;
    // Use defaults: totalHeight = 100, totalAngle = 19 * 2 * PI
    input.totalHeight = 23.25f; // Total height of the corkscrew in centimeters
    input.totalAngle = 19.0f * TWO_PI; // Default to 19 turns
    input.offsetCircumference = 0.0f;  // No offset
    input.numLeds = 288; // Default to dense 144 LEDs times two strips

    Corkscrew::Output output = Corkscrew::generateMap(input);

    // Basic sanity checks
    CHECK_EQ(output.width, 16);
    CHECK_EQ(output.height, 19);
    CHECK_EQ(output.mapping.size(), 288);

    // Check that circumference matches calculated value
    // float expectedCircumference = 100.0f / 19.0f;
    // CHECK_CLOSE(output.circumference, expectedCircumference, 0.01f);
}
