#pragma once


/*



# Corkscrew projection

  * Projection from Corkscrew (θ, h) to Cylindrical (θ, h)
    * Super sample cylindrical space
    * θ should in this case be normalized to [0, 1]?
    * Because cylindrical projection will be on a W X H grid, so θ -> [0, W-1]
  * This needs to be super sampled 2x2.
  * The further sampling will be done via the function currently in the XYPathRenderer
  * `Splat Rendering`
    * for "sub-pixel" (neighbor splatting?) rendering.
    * // Rasterizes point with a value For best visual results, you'll want to
    // rasterize tile2x2 tiles, which are generated for you by the XYPathRenderer
    // to represent sub pixel / neightbor splatting positions along a path.
    // TODO: Bring the math from XYPathRenderer::at_subpixel(float alpha)
    // into a general purpose function.


*Inputs*

  * Total Circumference / length of the Corkscrew
  * Total angle of the Corkscrew
    * Count the total number of segments in the vertical direction and times by 2pi, likewise with the ramainder.
  * Optional offset circumference, default 0.
    * Allows us to generate pixel perfect corkscrew with gaps acccounted for via circuference offseting.
    * Example (accounts for gap):
      * segment 0: offset circumference = 0, circumference = 100
      * segment 1: offset circumference = 100.5, circumference = 100
      * segment 2: offset circumference = 101, circumference = 100



*Outputs*

  * Width and and a Height of the cyclindracal map
    * Width is the circumference of one turn on the Corkscrew
    * Height is the total number of segments in the vertical direction
    * Some rounding up will be necessary for the projection.
  * The reference format will be a vector of vec2f {width, height} mapping each point on the corkscrew (r,c) to the cylindrical space {w,h}
  * Optional compact format is vec2u {width, height} vec2<uint8_t> for linear blend in each w and h direction.
    * This would keep the data computation in fixed integer space.
    * Is small enough to fit in a single uint32_t, upto 256x256 pixels (HUGE DISPLAY SEGMENT)


*/


#include "fl/math_macros.h"
#include "fl/vector.h"
#include "fl/geometry.h"



namespace fl {

using Vec2u8 = fl::vec2<uint8_t>;

struct Corkscrew {

    struct Input {
        float totalCircumference = 100;   // Length in centimeters
        float totalAngle = 19.f * 2 * PI; // Default to 19 turns
        float offsetCircumference = 0;
        bool compact = false;
        Input() = default;
    };

    struct Output {
        fl::vector<vec2f> mapping;
        uint16_t width;
        uint16_t height;
        fl::vector<Vec2u8> mappingCompact;
    };

    static void generateMap(const Input &input, Output &output);

    static Output generateMap(const Input &input) {
        Output output;
        generateMap(input, output);
        return output;
    }
};

}  // namespace fl
