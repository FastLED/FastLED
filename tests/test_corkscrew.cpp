// g++ --std=c++11 test.cpp

#include "fl/math_macros.h"
#include "test.h"

#include "fl/sstream.h"

#include "fl/corkscrew.h"

#define NUM_LEDS 288

#define TWO_PI (PI * 2.0)

using namespace fl;

TEST_CASE("Corkscrew generateMap") {
    Corkscrew::Input input;
    input.totalHeight = 10.0f;
    input.totalTurns = 1;
    input.offsetCircumference = 0.0f;
    input.numLeds = 10;

    Corkscrew::Output output = Corkscrew::generateMap(input);

    CHECK_EQ(output.width, 10);
    CHECK_EQ(output.height, 2);          // One vertical segment for one turn
    CHECK_EQ(output.mapping.size(), 10); // 10 LEDs around the corkscrew

    CHECK_GE(output.mapping[0].x, 0.0f);
    CHECK_LE(output.mapping[0].x, 10.0f);
    CHECK_GE(output.mapping[0].y, 0.0f);
    CHECK_LE(output.mapping[0].y, 1.0f); // 1 vertical segment for 2π angle
}

TEST_CASE("Corkscrew to Frame Buffer Mapping") {
    // Define the corkscrew input parameters
    const int kCorkscrewTotalHeight = 1; // cm
    // const int CORKSCREW_WIDTH = 1; // Width of the corkscrew in pixels
    // const int CORKSCREW_HEIGHT = 1; // Height of the corkscrew in pixels
    const int kCorkscrewTurns = 2; // Default to 19 turns

    Corkscrew::Input input;
    input.totalHeight = kCorkscrewTotalHeight;
    input.totalTurns = kCorkscrewTurns; // Default to 19 turns
    input.offsetCircumference = 0.0f;
    input.numLeds = 3;
    // Generate the corkscrew map
    Corkscrew corkscrew(input);

    volatile Corkscrew::Output *output = &corkscrew.access();

    // vec2<int16_t> first = corkscrew.at(0);
    // vec2<int16_t> second = corkscrew.at(1);

    Corkscrew::iterator it = corkscrew.begin();
    Corkscrew::iterator end = corkscrew.end();

    fl::sstream ss;
    ss << "\n";
    ss << "width: " << output->width << "\n";
    ss << "height: " << output->height << "\n";

    while (it != end) {
        ss << *it << "\n";
        ++it;
    }

    FASTLED_WARN(ss.str());

    MESSAGE("done");
}

TEST_CASE("Corkscrew generateMap with two turns") {
    Corkscrew::Input input;
    input.totalHeight = 10.0f;
    input.totalTurns = 2; // Two full turns
    input.numLeds = 10;   // 10 LEDs around the corkscrew
    input.offsetCircumference = 0.0f;

    Corkscrew::Output output = Corkscrew::generateMap(input);

    CHECK_EQ(output.width, 6);
    CHECK_EQ(output.height, 3);          // Two vertical segments for two turns
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
    input.totalTurns = 19.0f;   // Default to 19 turns
    input.offsetCircumference = 0.0f; // No offset
    input.numLeds = 288; // Default to dense 144 LEDs times two strips

    Corkscrew::Output output = Corkscrew::generateMap(input);

    // Basic sanity checks
    CHECK_EQ(output.width, 3);
    CHECK_EQ(output.height, 20);
    CHECK_EQ(output.mapping.size(), 288);

    // Check that circumference matches calculated value
    // float expectedCircumference = 100.0f / 19.0f;
    // CHECK_CLOSE(output.circumference, expectedCircumference, 0.01f);
}
