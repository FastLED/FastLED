// g++ --std=c++11 test.cpp

#include "test.h"

#include <FastLED.h>
#include "fl/vector.h"

#include "fl/math_macros.h"

#define TWO_PI (PI * 2.0)

using namespace fl;

struct Vec2f {
    float x, y;
};

struct Vec2u8 {
    uint8_t x, y;
};

struct CorkscrewProjectionInput {
    float totalCircumference = 100;  // Length in centimeters
    float totalAngle = 19.f * TWO_PI; // Default to 19 turns
    float offsetCircumference = 0;
    bool compact = false;
};

struct CorkscrewProjectionOutput {
    fl::vector<Vec2f> mapping;
    uint16_t width;
    uint16_t height;
    fl::vector<Vec2u8> mappingCompact;
};

struct CorkscrewProjection {
    static void run(const CorkscrewProjectionInput& input, 
                   CorkscrewProjectionOutput& output);
};

// Corkscrew-to-cylindrical projection function
void CorkscrewProjection::run(const CorkscrewProjectionInput& input,
                              CorkscrewProjectionOutput& output) {
    // Calculate vertical segments based on number of turns
    // For a single turn (2π), we want exactly 1 vertical segment
    // For two turns (4π), we want exactly 2 vertical segments
    uint16_t verticalSegments = round(input.totalAngle / TWO_PI);
    
    // Determine cylindrical dimensions
    output.height = verticalSegments;
    output.width = ceil(input.totalCircumference);

    output.mapping.clear();
    output.mapping.reserve(output.width * output.height);

    // Corrected super sampling step size
    float thetaStep = 0.5f / output.width;
    float hStep = 0.5f / output.height;

    // Precompute angle per segment
    float anglePerSegment = input.totalAngle / verticalSegments;

    // Loop over cylindrical pixels
    for (uint16_t h = 0; h < output.height; ++h) {
        float segmentOffset = input.offsetCircumference * h;
        for (uint16_t w = 0; w < output.width; ++w) {
            Vec2f sample = {0, 0};
            // 2x2 supersampling
            for (uint8_t ssH = 0; ssH < 2; ++ssH) {
                for (uint8_t ssW = 0; ssW < 2; ++ssW) {
                    float theta = (w + 0.5f + ssW * thetaStep) / output.width;
                    float height = (h + 0.5f + ssH * hStep) / output.height;

                    // Corkscrew projection (θ,h)
                    float corkscrewTheta = theta * TWO_PI + anglePerSegment * h;
                    float corkscrewH = height * verticalSegments;

                    // Apply circumference offset
                    float corkscrewCircumference =
                        fmodf(corkscrewTheta * input.totalCircumference / TWO_PI +
                                  segmentOffset,
                              input.totalCircumference);

                    // Accumulate samples
                    sample.x += corkscrewCircumference;
                    sample.y += corkscrewH;
                }
            }

            // Average the supersampled points
            sample.x *= 0.25f;
            sample.y *= 0.25f;

            output.mapping.push_back(sample);

            // Optionally compact the mapping into Vec2u8 format
            if (input.compact) {
                Vec2u8 compactSample = {
                    static_cast<uint8_t>((sample.x / input.totalCircumference) * 255),
                    static_cast<uint8_t>((sample.y / verticalSegments) * 255)};
                output.mappingCompact.push_back(compactSample);
            }
        }
    }
}

TEST_CASE("Corkscrew project") {
    CorkscrewProjectionInput input;
    input.totalCircumference = 10.0f;
    input.totalAngle = TWO_PI;
    input.offsetCircumference = 0.0f;
    input.compact = false;

    CorkscrewProjectionOutput output;

    CorkscrewProjection::run(input, output);

    CHECK(output.width == 10);
    CHECK(output.height == 1);
    CHECK(output.mapping.size() == 10);

    // Check first pixel for correctness (basic integrity)
    CHECK(output.mapping[0].x >= 0.0f);
    CHECK(output.mapping[0].x <= input.totalCircumference);
    CHECK(output.mapping[0].y >= 0.0f);
    CHECK(output.mapping[0].y <= 1.0f); // 1 vertical segment for 2π angle
}

TEST_CASE("Corkscrew project with two turns") {
    CorkscrewProjectionInput input;
    input.totalCircumference = 10.0f;
    input.totalAngle = 2 * TWO_PI; // Two full turns
    input.offsetCircumference = 0.0f;
    input.compact = false;

    CorkscrewProjectionOutput output;

    CorkscrewProjection::run(input, output);

    CHECK(output.width == 10);
    CHECK(output.height == 2); // Two vertical segments for two turns
    CHECK(output.mapping.size() == 20); // 10 width * 2 height

    // Check first pixel for correctness (basic integrity)
    CHECK(output.mapping[0].x >= 0.0f);
    CHECK(output.mapping[0].x <= input.totalCircumference);
    CHECK(output.mapping[0].y >= 0.0f);
    CHECK(output.mapping[0].y <= 2.0f); // 2 vertical segments for 4π angle
}
