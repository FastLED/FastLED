// g++ --std=c++11 test.cpp

#include "test.h"

#include <FastLED.h>
#include <vector>

#include "fl/math_macros.h"

#define TWO_PI (PI * 2.0)

using namespace fl;

struct Vec2f {
    float x, y;
};

struct Vec2u8 {
    uint8_t x, y;
};

struct CorkscrewProjection {

    static void run(float totalCircumference, float totalAngle,
                    uint16_t verticalSegments, float offsetCircumference,
                    std::vector<Vec2f> &outMapping, uint16_t &outWidth,
                    uint16_t &outHeight, bool compact = false,
                    std::vector<Vec2u8> *outMappingCompact = nullptr);
};

// Corkscrew-to-cylindrical projection function
void CorkscrewProjection::run(float totalCircumference, float totalAngle,
                              uint16_t verticalSegments,
                              float offsetCircumference,
                              std::vector<Vec2f> &outMapping,
                              uint16_t &outWidth, uint16_t &outHeight,
                              bool compact,
                              std::vector<Vec2u8> *outMappingCompact) {
    // Determine cylindrical dimensions
    outHeight = verticalSegments;
    outWidth = ceil(totalCircumference);

    outMapping.clear();
    outMapping.reserve(outWidth * outHeight);

    // Corrected super sampling step size
    float thetaStep = 0.5f / outWidth;
    float hStep = 0.5f / outHeight;

    // Precompute angle per segment
    float anglePerSegment = totalAngle / verticalSegments;

    // Loop over cylindrical pixels
    for (uint16_t h = 0; h < outHeight; ++h) {
        float segmentOffset = offsetCircumference * h;
        for (uint16_t w = 0; w < outWidth; ++w) {
            Vec2f sample = {0, 0};
            // 2x2 supersampling
            for (uint8_t ssH = 0; ssH < 2; ++ssH) {
                for (uint8_t ssW = 0; ssW < 2; ++ssW) {
                    float theta = (w + 0.5f + ssW * thetaStep) / outWidth;
                    float height = (h + 0.5f + ssH * hStep) / outHeight;

                    // Corkscrew projection (θ,h)
                    float corkscrewTheta = theta * TWO_PI + anglePerSegment * h;
                    float corkscrewH = height * verticalSegments;

                    // Apply circumference offset
                    float corkscrewCircumference =
                        fmodf(corkscrewTheta * totalCircumference / TWO_PI +
                                  segmentOffset,
                              totalCircumference);

                    // Accumulate samples
                    sample.x += corkscrewCircumference;
                    sample.y += corkscrewH;
                }
            }

            // Average the supersampled points
            sample.x *= 0.25f;
            sample.y *= 0.25f;

            outMapping.push_back(sample);

            // Optionally compact the mapping into Vec2u8 format
            if (compact && outMappingCompact) {
                Vec2u8 compactSample = {
                    static_cast<uint8_t>((sample.x / totalCircumference) * 255),
                    static_cast<uint8_t>((sample.y / verticalSegments) * 255)};
                outMappingCompact->push_back(compactSample);
            }
        }
    }
}

TEST_CASE("Corkscrew project") {
    float totalCircumference = 10.0f;
    float totalAngle = TWO_PI;
    uint16_t verticalSegments = 1;
    float offsetCircumference = 0.0f;

    std::vector<Vec2f> mapping;
    uint16_t width = 0;
    uint16_t height = 0;

    CorkscrewProjection::run(totalCircumference, totalAngle, verticalSegments,
                        offsetCircumference, mapping, width, height);

    CHECK(width == 10);
    CHECK(height == 1);
    CHECK(mapping.size() == 10);

    // Check first pixel for correctness (basic integrity)
    CHECK(mapping[0].x >= 0.0f);
    CHECK(mapping[0].x <= totalCircumference);
    CHECK(mapping[0].y >= 0.0f);
    CHECK(mapping[0].y <= verticalSegments);
}
