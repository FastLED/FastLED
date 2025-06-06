#include "fl/corkscrew.h"
#include "fl/algorithm.h"
#include "fl/math.h"
#include "fl/splat.h"

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
    const float max_i_width = float(input.numLeds - 1);  // Corkscrew reaches max width on last led.
    const float max_i_height = float(input.numLeds - 1);  // Corkscrew reaches max height on last led.
    // const float led_width_factor = circumferencePerTurn / TWO_PI;
    const float leds_per_turn = input.numLeds / input.totalTurns;

    for (uint16_t i = 0; i < input.numLeds; ++i) {
        // Calculate position along the corkscrew (0.0 to 1.0)
        const float alpha_width = static_cast<float>(i) / max_i_width;
        const float alpha_height = static_cast<float>(i) / max_i_height;
        const float width_before_mod = alpha_width * input.totalLength;
        const float height = alpha_height * input.totalHeight;
        const float width = fmodf(width_before_mod, leds_per_turn);
        output->mapping.push_back({width, height});
    }

    if (!output->mapping.empty()) {
        float max_width =
            fl::max_element(
                output->mapping.begin(), output->mapping.end(),
                [](const vec2f &a, const vec2f &b) { return a.x < b.x; })
                ->x;
        float max_height =
            fl::max_element(
                output->mapping.begin(), output->mapping.end(),
                [](const vec2f &a, const vec2f &b) { return a.y < b.y; })
                ->y;
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
