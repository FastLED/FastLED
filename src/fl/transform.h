/*
Efficient transform classes for floating point and alpha16 coordinate systems.
Note that component transforms are used because it's easy to skip calculations
for components that are not used. For example, if the rotation is 0 then no
expensive trig functions are needed. Same with scale and offset.

*/

#include "lib8tion/types.h"
#include "fl/lut.h"
#include "fl/xymap.h"


namespace fl {

using alpha16 =
    uint16_t; // fixed point representation of 0->1 in the range [0, 65535]

// This transform assumes the coordinates are in the range [0,65535].
struct Transform16 {
    // Make a transform that maps a rectangle to the given bounds from
    // (0,0) to (max_value,max_value), inclusive.
    static Transform16 ToBounds(alpha16 max_value);
    static Transform16 ToBounds(const point_xy<alpha16> &min,
                                const point_xy<alpha16> &max,
                                alpha16 rotation = 0);

    static Transform16 From(uint16_t width, uint16_t height) {
        point_xy<alpha16> min = point_xy<alpha16>(0,0);
        point_xy<alpha16> max = point_xy<alpha16>(width, height);
        return Transform16::ToBounds(min, max);
    }

    // static Transform16 From(const XYMap &map) {
    //     return Transform16::ToBounds(map.getWidth(), map.getHeight());
    // }
    Transform16() = default;
    alpha16 scale_x = 0xffff;
    alpha16 scale_y = 0xffff;
    alpha16 x_offset = 0;
    alpha16 y_offset = 0;
    alpha16 rotation = 0;

    point_xy<alpha16> transform(const point_xy<alpha16> &xy) const;
};

// This transform assumes the coordinates are in the range [0,1].
struct TransformFloat {
    TransformFloat() = default;
    float scale_x = 1.0f;  // 0 -> 1
    float scale_y = 1.0f;  // 0 -> 1
    float x_offset = 0.0f; // 0 -> 1
    float y_offset = 0.0f; // 0 -> 1
    float rotation = 0.0f; // 0 -> 1
    point_xy_float transform(const point_xy_float &xy) const;
};

} // namespace fl