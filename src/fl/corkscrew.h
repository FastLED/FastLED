#pragma once

/**
 * @file corkscrew.h
 * @brief Corkscrew projection utilities
 *
 * Corkscrew projection maps from Corkscrew (θ, h) to Cylindrical cartesian (w, h)
 * space, where w = one turn of the Corkscrew. The the corkscrew at (0,0) will
 * map to (0,0) in cylindrical space.
 *
 * The projection:
 * - Super samples cylindrical space
 * - θ is normalized to [0, 1] or mapped to [0, W-1] for grid projection
 * - Uses 2x2 super sampling for better visual quality
 * - Works with XYPathRenderer's "Splat Rendering" for sub-pixel rendering
 *
 * Inputs:
 * - Total Circumference/length of the Corkscrew
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
 * - Optional compact format using vec2<uint8_t> for linear blending
 *   - Keeps computation in fixed integer space
 *   - Fits in a single uint32_t (up to 256×256 pixels)
 */

#include "fl/geometry.h"
#include "fl/math_macros.h"
#include "fl/vector.h"
#include "fl/allocator.h"

namespace fl {

struct Corkscrew {

    /**
     * Input parameters for corkscrew projection
     */
    struct Input {
        float totalCircumference = 100;   // Length in centimeters
        float totalAngle = 19.f * 2 * PI; // Default to 19 turns
        float offsetCircumference = 0;    // Optional offset for gap accounting
        bool compact = false; // Whether to use compact representation
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
        fl::vector<fl::vec2f>
            mapping; // Full precision mapping from corkscrew to cylindrical
        fl::vector<fl::vec2u8>
            mappingCompact; // Compact mapping for fixed integer computation
    };

    /**
     * Generates a mapping from corkscrew to cylindrical coordinates
     * @param input The input parameters defining the corkscrew
     * @param output The resulting cylindrical mapping
     */
    static void generateMap(const Input &input, Output &output);

    static Output generateMap(const Input &input) {
        Output output;
        generateMap(input, output);
        return output;
    }
};

} // namespace fl
