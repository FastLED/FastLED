#include "fl/corkscrew.h"
#include "fl/math.h"

namespace fl {

// Corkscrew-to-cylindrical projection function
void Corkscrew::generateMap(const Corkscrew::Input &input,
                            Corkscrew::Output &output) {
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
                    float corkscrewCircumference = fmodf(
                        corkscrewTheta * input.totalCircumference / TWO_PI +
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
                    static_cast<uint8_t>((sample.x / input.totalCircumference) *
                                         255),
                    static_cast<uint8_t>((sample.y / verticalSegments) * 255)};
                output.mappingCompact.push_back(compactSample);
            }
        }
    }
}

} // namespace fl
