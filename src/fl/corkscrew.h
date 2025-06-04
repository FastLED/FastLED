#pragma once

/**
 * @file corkscrew.h
 * @brief Corkscrew projection utilities
 *
 * Corkscrew projection maps from Corkscrew (θ, h) to Cylindrical cartesian (w, h)
 * space, where w = one turn of the Corkscrew. The corkscrew at (0,0) will
 * map to (0,0) in cylindrical space.
 *
 * The projection:
 * - Super samples cylindrical space?
 * - θ is normalized to [0, 1] or mapped to [0, W-1] for grid projection
 * - Uses 2x2 super sampling for better visual quality
 * - Works with XYPathRenderer's "Splat Rendering" for sub-pixel rendering
 *
 * Inputs:
 * - Total Height of the Corkscrew in centimeters
 * - Total angle of the Corkscrew (number of veritcal segments × 2π)
 * - Optional offset circumference (default 0)
 *   - Allows pixel-perfect corkscrew with gaps via circumference offsetting
 *   - Example (accounting for gaps):
 *     - segment 0: offset circumference = 0, circumference = 100
 *     - segment 1: offset circumference = 100.5, circumference = 100
 *     - segment 2: offset circumference = 101, circumference = 100
 *
 * Outputs:
 * - Width and Height of the cylindrical map
 *   - Width is the circumference of one turn
 *   - Height is the total number of vertical segments
 * - Vector of vec2f {width, height} mapping corkscrew (r,c) to cylindrical
 * {w,h}
 */

#include "fl/geometry.h"
#include "fl/math_macros.h"
#include "fl/vector.h"
#include "fl/allocator.h"

namespace fl {

struct Corkscrew {

    /**
     * Input parameters for corkscrew projection
     * - totalHeight: Total height of the corkscrew in centimeters.
     * - totalAngle: Total angle of the corkscrew, defaulting to 19 turns (19 * 2π).
     * - offsetCircumference: Optional offset for gap accounting between segments.
     * - numLeds: Number of LEDs in the strip.
     * - width: Width of cylindrical map (circumference of one turn).
     * - height: Height of cylindrical map (total vertical segments).
     * - mapping: Full precision mapping from corkscrew to cylindrical coordinates,
     *   stored in a vector with a PSRAM allocator for efficient memory usage.
     */
    struct Input {
        float totalHeight = 23.25;        // Total height of the corkscrew in centimeters for 144 densly wrapped up over 19 turns
        float totalAngle = 19.f * 2 * PI; // Default to 19 turns
        float offsetCircumference = 0;    // Optional offset for gap accounting
        uint16_t numLeds = 144;           // Default to dense 144 leds.
        Input() = default;
    };

    /**
     * Output data from corkscrew projection
     */
    struct Output {
        uint16_t width =
            0; // Width of cylindrical map (circumference of one turn)
        uint16_t height =
            0; // Height of cylindrical map (total vertical segments)
        fl::vector<fl::vec2f, fl::allocator_psram<fl::vec2f>>
            mapping; // Full precision mapping from corkscrew to cylindrical
        Output() = default;
    };

    /**
     * Generates a mapping from corkscrew to cylindrical coordinates
     * @param input The input parameters defining the corkscrew.
     * @param output The resulting cylindrical mapping, modified in place.
     */
    static void generateMap(const Input &input, Output &output);

    /**
     * Generates a mapping from corkscrew to cylindrical coordinates and returns it.
     * @param input The input parameters defining the corkscrew.
     * @return The resulting cylindrical mapping.
     */
    static Output generateMap(const Input &input) {
        Output output;
        generateMap(input, output);
        return output;
    }
};

} // namespace fl
