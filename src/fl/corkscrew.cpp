#include "fl/corkscrew.h"
#include "fl/algorithm.h"
#include "fl/math.h"
#include "fl/splat.h"

#define TWO_PI (PI * 2.0)

namespace fl {

// LEGACY - TODO - fold this into a free functions
struct CorkscrewLegacy {

    /**
     * Input parameters for corkscrew projection
     * - totalHeight: Total height of the corkscrew in centimeters.
     * - totalAngle: Total angle of the corkscrew, defaulting to 19 turns (19 *
     * 2π).
     * - offsetCircumference: Optional offset for gap accounting between
     * segments.
     * - numLeds: Number of LEDs in the strip.
     * - width: Width of cylindrical map (circumference of one turn).
     * - height: Height of cylindrical map (total vertical segments).
     * - mapping: Full precision mapping from corkscrew to cylindrical
     * coordinates, stored in a vector with a PSRAM allocator for efficient
     * memory usage.
     */

    using Input = Corkscrew::Input;
    using Output = CorkscrewOutput;

    /**
     * Output data from corkscrew projection
     */

    /**
     * Generates a mapping from corkscrew to cylindrical coordinates
     * @param input The input parameters defining the corkscrew.
     * @param output The resulting cylindrical mapping, modified in place.
     */
    static void generateMap(const Input &input, Output &output);

    /**
     * Generates a mapping from corkscrew to cylindrical coordinates and returns
     * it.
     * @param input The input parameters defining the corkscrew.
     * @return The resulting cylindrical mapping.
     */
    static Output generateMap(const Input &input) {
        Output output;
        generateMap(input, output);
        return output;
    }
};

// Corkscrew-to-cylindrical projection function
void CorkscrewLegacy::generateMap(const Corkscrew::Input &input,
                                  CorkscrewLegacy::Output &output) {
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
            float circumference = fmodf(angle * circumferencePerTurn / TWO_PI,
                                        circumferencePerTurn);

            // Store the mapping
            output.mapping.push_back({circumference, height});
        }
    } else {
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
                        float theta =
                            (w + 0.5f + ssW * thetaStep) / output.width;
                        float height = (h + 0.5f + ssH * hStep) / output.height;

                        // Corkscrew projection (θ,h)
                        float corkscrewTheta =
                            theta * TWO_PI + anglePerSegment * h;
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

Tile2x2_u8 CorkscrewLegacy::Output::at(int16_t x, int16_t y) const {
    // Ensure x and y are within bounds
    if (x < 0 || x >= width || y < 0 || y >= height) {
        // Handle out-of-bounds access, possibly by returning a default
        // Tile2x2_u8
        return Tile2x2_u8();
    }

    // Calculate the index in the mapping vector
    uint16_t index = y * width + x;

    // Retrieve the vec2f from the mapping
    vec2f position = mapping[index];

    // Use the splat function to convert the vec2f to a Tile2x2_u8
    Tile2x2_u8 tile = splat(position);
    return tile;
}

Corkscrew::Corkscrew(const Corkscrew::Input &input) : mInput(input) {
    // Generate the mapping using the existing CorkscrewLegacy::generateMap
    // function
    CorkscrewLegacy::generateMap(mInput, mOutput);
}

vec2<int16_t> Corkscrew::at(uint16_t i) const {
    if (i >= mOutput.mapping.size()) {
        // Handle out-of-bounds access, possibly by returning a default value
        return vec2<int16_t>(0, 0);
    }
    // Convert the float position to integer
    const vec2f &position = mOutput.mapping[i];
    return vec2<int16_t>(static_cast<int16_t>(position.x),
                         static_cast<int16_t>(position.y));
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
    CorkscrewLegacy::generateMap(input, output);
    return output;
}

} // namespace fl
