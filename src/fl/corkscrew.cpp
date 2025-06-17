#include "fl/corkscrew.h"
#include "fl/algorithm.h"
#include "fl/assert.h"
#include "fl/math.h"
#include "fl/splat.h"
#include "fl/warn.h"

#include "fl/math_macros.h"

#define TWO_PI (PI * 2.0)

namespace fl {

void generateState(const Corkscrew::Input &input, CorkscrewState *output);

void generateState(const Corkscrew::Input &input, CorkscrewState *output) {

    output->mapping.clear();
    output->width = input.calculateWidth();
    output->height = input.calculateHeight();

    // Generate LED mapping based on numLeds
    output->mapping.reserve(input.numLeds);
    
    for (uint16_t i = 0; i < input.numLeds; ++i) {
        // Calculate position along the corkscrew (0.0 to 1.0)
        const float ledProgress = static_cast<float>(i) / static_cast<float>(input.numLeds - 1);
        
        // Calculate angle position (0 to totalTurns * 2π)
        const float totalAngle = ledProgress * input.totalTurns * TWO_PI;
        const float normalizedAngle = fmodf(totalAngle, TWO_PI) / TWO_PI; // 0 to 1 within current turn
        
        // Calculate height position (0 to height-1)
        const float heightProgress = ledProgress; // Linear progression through height
        
        // Map to grid coordinates
        const float width_pos = normalizedAngle * static_cast<float>(output->width - 1) + input.offsetCircumference;
        const float height_pos = heightProgress * static_cast<float>(output->height - 1);
        
        // Handle width wrapping for offset circumference
        const float final_width = fmodf(width_pos, static_cast<float>(output->width));
        
        output->mapping.push_back({final_width, height_pos});
    }

    // Apply inversion if requested
    if (input.invert) {
        fl::reverse(output->mapping.begin(), output->mapping.end());
    }
}

Corkscrew::Corkscrew(const Corkscrew::Input &input) : mInput(input) {
    fl::generateState(mInput, &mState);
}

vec2f Corkscrew::at_exact(uint16_t i) const {
    if (i >= mState.mapping.size()) {
        // Handle out-of-bounds access, possibly by returning a default value
        return vec2f(0, 0);
    }
    // Convert the float position to integer
    const vec2f &position = mState.mapping[i];
    return position;
}


Tile2x2_u8 Corkscrew::at_splat_extrapolate(float i) const {
    // To finish this, we need to handle wrap around.
    // To accomplish this we need a different data structure than the the
    // Tile2x2_u8.
    // 1. It will be called CorkscrewTile2x2_u8.
    // 2. The four alpha values will each contain the index the LED is at,
    // uint16_t.
    // 3. There will be no origin, each pixel in the tile will contain a
    // uint16_t origin. This is not supposed to be a storage format, but a
    // convenient pre-computed value for rendering.
    if (i >= mState.mapping.size()) {
        // Handle out-of-bounds access, possibly by returning a default
        // Tile2x2_u8
        FASTLED_ASSERT(false, "Out of bounds access in Corkscrew at_splat: "
                                  << i << " size: " << mState.mapping.size());
        return Tile2x2_u8();
    }
    // Use the splat function to convert the vec2f to a Tile2x2_u8
    float i_floor = floorf(i);
    float i_ceil = ceilf(i);
    if (ALMOST_EQUAL_FLOAT(i_floor, i_ceil)) {
        // If the index is the same, just return the splat of that index
        return splat(mState.mapping[static_cast<uint16_t>(i_floor)]);
    } else {
        // Interpolate between the two points and return the splat of the result
        vec2f pos1 = mState.mapping[static_cast<uint16_t>(i_floor)];
        vec2f pos2 = mState.mapping[static_cast<uint16_t>(i_ceil)];

        if (pos2.x < pos1.x) {
            // If the next point is on the other side of the cylinder, we need
            // to wrap it around and bring it back into the positive direction so we can construct a Tile2x2_u8 wrap with it.
            pos2.x += mState.width;
        }

        vec2f interpolated_pos =
            pos1 * (1.0f - (i - i_floor)) + pos2 * (i - i_floor);
        return splat(interpolated_pos);
    }
}

size_t Corkscrew::size() const { return mState.mapping.size(); }

Corkscrew::State Corkscrew::generateState(const Corkscrew::Input &input) {
    CorkscrewState output;
    fl::generateState(input, &output);
    return output;
}



Tile2x2_u8_wrap Corkscrew::at_wrap(float i) const {
    // This is a splatted pixel, but wrapped around the cylinder.
    // This is useful for rendering the corkscrew in a cylindrical way.
    Tile2x2_u8 tile = at_splat_extrapolate(i);
    return Tile2x2_u8_wrap(tile, mState.width, mState.height);
}

} // namespace fl