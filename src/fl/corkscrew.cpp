#include "fl/corkscrew.h"
#include "fl/algorithm.h"
#include "fl/math.h"
#include "fl/splat.h"

#define TWO_PI (PI * 2.0)

namespace fl {

void generateMap(const Corkscrew::Input &input, CorkscrewOutput &output);

void generateMap(const Corkscrew::Input &input, CorkscrewOutput &output) {
    // Calculate circumference per turn from height and total angle
    float circumferencePerTurn = input.totalHeight / input.totalTurns;

    // Calculate vertical segments based on number of turns
    // For a single turn (2π), we want exactly 1 vertical segment
    // For two turns (4π), we want exactly 2 vertical segments
    uint16_t verticalSegments = round(input.totalTurns);

    // Calculate width based on LED density per turn
    // float ledsPerTurn = static_cast<float>(input.numLeds) / verticalSegments;

    // Determine cylindrical dimensions
    // output.height = verticalSegments;
    // output.width = ceil(ledsPerTurn);
    output.height = output.width = 0;  // we will change this below.

    output.mapping.clear();

    // If numLeds is specified, use that for mapping size instead of grid
    output.mapping.reserve(input.numLeds);
    // Generate LED mapping based on numLeds
    for (uint16_t i = 0; i < input.numLeds; ++i) {
        // Calculate position along the corkscrew (0.0 to 1.0)
        float position = static_cast<float>(i) / (input.numLeds - 1);

        // Calculate angle and height
        float angle = position * input.totalTurns * 2 * PI;
        float height = position * verticalSegments;

        // Calculate circumference position
        float circumference = fmodf(angle * circumferencePerTurn / TWO_PI,
                                    circumferencePerTurn);

        // Store the mapping
        output.mapping.push_back({circumference, height});
    }

    if (!output.mapping.empty()) {
        float max_width = fl::max_element(output.mapping.begin(), output.mapping.end(),
                                           [](const vec2f &a, const vec2f &b) {
                                               return a.x < b.x;
                                           })->x;
        float max_height = fl::max_element(output.mapping.begin(), output.mapping.end(),
                                            [](const vec2f &a, const vec2f &b) {
                                                return a.y < b.y;
                                            })->y;
        output.width = static_cast<uint16_t>(ceilf(max_width)) + 1;
        output.height = static_cast<uint16_t>(ceilf(max_height)) + 1;
    }


    // Apply inversion if requested
    if (input.invert) {
        fl::reverse(output.mapping.begin(), output.mapping.end());
    }
}


Corkscrew::Corkscrew(const Corkscrew::Input &input) : mInput(input) {
    fl::generateMap(mInput, mOutput);
}

vec2f Corkscrew::at(uint16_t i) const {
    if (i >= mOutput.mapping.size()) {
        // Handle out-of-bounds access, possibly by returning a default value
        return vec2f(0, 0);
    }
    // Convert the float position to integer
    const vec2f &position = mOutput.mapping[i];
    return position;
}

Tile2x2_u8 Corkscrew::at_splat(uint16_t i) const {
    if (i >= mOutput.mapping.size()) {
        // Handle out-of-bounds access, possibly by returning a default
        // Tile2x2_u8
        return Tile2x2_u8();
    }
    // Use the splat function to convert the vec2f to a Tile2x2_u8
    return splat(mOutput.mapping[i]);
}

size_t Corkscrew::size() const { return mOutput.mapping.size(); }

CorkscrewOutput Corkscrew::generateMap(const Input &input) {
    CorkscrewOutput output;
    fl::generateMap(input, output);
    return output;
}

} // namespace fl
