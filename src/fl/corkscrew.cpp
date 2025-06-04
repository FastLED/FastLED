#include "fl/corkscrew.h"
#include "fl/math.h"
#include "fl/algorithm.h"

#define TWO_PI (PI * 2.0)

namespace fl {

// Corkscrew-to-cylindrical projection function
void Corkscrew::generateMap(const Corkscrew::Input &input,
                            Corkscrew::Output &output) {
    // Calculate circumference per turn from height and total angle
    float circumferencePerTurn = input.totalHeight * TWO_PI / input.totalAngle;
    
    // Calculate vertical segments based on number of turns
    // For a single turn (2π), we want exactly 1 vertical segment
    // For two turns (4π), we want exactly 2 vertical segments
    uint16_t verticalSegments = round(input.totalAngle / TWO_PI);

    // Calculate width based on LED density per turn
    float ledsPerTurn = static_cast<float>(input.numLeds) / verticalSegments;
    
    // Determine cylindrical dimensions
    output.height = verticalSegments;
    output.width = ceil(ledsPerTurn);

    output.mapping.clear();
    
    // If numLeds is specified, use that for mapping size instead of grid
    if (input.numLeds > 0) {
        output.mapping.reserve(input.numLeds);
        
        // Generate LED mapping based on numLeds
        for (uint16_t i = 0; i < input.numLeds; ++i) {
            // Calculate position along the corkscrew (0.0 to 1.0)
            float position = static_cast<float>(i) / (input.numLeds - 1);
            
            // Calculate angle and height
            float angle = position * input.totalAngle;
            float height = position * verticalSegments;
            
            // Calculate circumference position
            float circumference = fmodf(
                angle * circumferencePerTurn / TWO_PI,
                circumferencePerTurn);
            
            // Store the mapping
            output.mapping.push_back({circumference, height});
        }
    } 
    else {
        // Original grid-based mapping
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
                vec2f sample = {0, 0};
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
                            corkscrewTheta * circumferencePerTurn / TWO_PI +
                                segmentOffset,
                            circumferencePerTurn);

                        // Accumulate samples
                        sample.x += corkscrewCircumference;
                        sample.y += corkscrewH;
                    }
                }

                // Average the supersampled points
                sample.x *= 0.25f;
                sample.y *= 0.25f;

                output.mapping.push_back(sample);
            }
        }
    }
    
    // Apply inversion if requested
    if (input.invert) {
        fl::reverse(output.mapping.begin(), output.mapping.end());
    }
}

} // namespace fl
