#include "fl/corkscrew.h"
#include "fl/algorithm.h"
#include "fl/math.h"
#include "fl/splat.h"
#include "fl/assert.h"

#include "fl/math_macros.h"

#define TWO_PI (PI * 2.0)

namespace fl {

void generateState(const Corkscrew::Input &input, CorkscrewState *output);

void generateState(const Corkscrew::Input &input, CorkscrewState *output) {

    // Calculate vertical segments based on number of turns
    // For a single turn (2π), we want exactly 1 vertical segment
    // For two turns (4π), we want exactly 2 vertical segments
    // uint16_t verticalSegments = ceil(input.totalTurns);

    // Calculate width based on LED density per turn
    // float ledsPerTurn = static_cast<float>(input.numLeds) / verticalSegments;

    output->mapping.clear();
    output->width = 0; // we will change this below.
    output->height = 0;

    // If numLeds is specified, use that for mapping size instead of grid
    output->mapping.reserve(input.numLeds);
    // Generate LED mapping based on numLeds
    // Note that width_step should be 1.0f/float(input.numLeds) so last led in a turn
    // does not wrap around.
    const float width_step = 1.0f / float(input.numLeds);  // Corkscrew reaches max width on last led.
    const float height_step = 1.0f / float(input.numLeds - 1);  // Corkscrew reaches max height on last led.
    // const float led_width_factor = circumferencePerTurn / TWO_PI;
    const float length_per_turn = input.totalLength / input.totalTurns;

    for (uint16_t i = 0; i < input.numLeds; ++i) {
        // Calculate position along the corkscrew (0.0 to 1.0)
        const float i_f = static_cast<float>(i);
        const float alpha_width = i_f * width_step;
        const float alpha_height = i_f * height_step;
        const float width_before_mod = alpha_width * input.totalLength;
        const float height = alpha_height * input.totalHeight;
        const float width = fmodf(width_before_mod, length_per_turn);
        output->mapping.push_back({width, height});
    }

    if (!output->mapping.empty()) {
        float max_width = 0.0f;
        float max_height = 0.0f;
        for (const auto &point : output->mapping) {
            max_width = MAX(max_width, point.x);
            max_height = MAX(max_height, point.y);
        }
        output->width = static_cast<uint16_t>(ceilf(max_width)) + 1;
        output->height = static_cast<uint16_t>(ceilf(max_height)) + 1;
    }

    // Apply inversion if requested
    if (input.invert) {
        fl::reverse(output->mapping.begin(), output->mapping.end());
    }
}

Corkscrew::Corkscrew(const Corkscrew::Input &input) : mInput(input) {
    fl::generateState(mInput, &mState);
}

vec2f Corkscrew::at(uint16_t i) const {
    if (i >= mState.mapping.size()) {
        // Handle out-of-bounds access, possibly by returning a default value
        return vec2f(0, 0);
    }
    // Convert the float position to integer
    const vec2f &position = mState.mapping[i];
    return position;
}

Tile2x2_u8 Corkscrew::at_splat(uint16_t i) const {
    if (i >= mState.mapping.size()) {
        // Handle out-of-bounds access, possibly by returning a default
        // Tile2x2_u8
        FASTLED_ASSERT(false, "Out of bounds access in Corkscrew at_splat: " << i
                        << " size: " << mState.mapping.size());
        return Tile2x2_u8();
    }
    // Use the splat function to convert the vec2f to a Tile2x2_u8
    return splat(mState.mapping[i]);
}

size_t Corkscrew::size() const { return mState.mapping.size(); }

Corkscrew::State Corkscrew::generateState(const Corkscrew::Input &input) {
    CorkscrewState output;
    fl::generateState(input, &output);
    return output;
}

} // namespace fl
