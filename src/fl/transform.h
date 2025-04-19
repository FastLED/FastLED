/*
Efficient transform classes for floating point and fract16 coordinate systems.
Note that component transforms are used because it's easy to skip calculations
for components that are not used. For example, if the rotation is 0 then no
expensive trig functions are needed. Same with scale and offset.

*/


#include "fl/lut.h"
#include "lib8tion/types.h"

namespace fl {

// This transform assumes the coordinates are in the range [0,1].
struct TransformFloat {
    TransformFloat() = default;
    float scale_x = 1.0f;
    float scale_y = 1.0f;
    float x_offset = 0.0f;
    float y_offset = 0.0f;
    float rotation = 0.0f;
    point_xy_float transform(const point_xy_float &xy) const;
};

// This transform assumes the coordinates are in the range [0,65535].
struct Transform16 {
    // Make a transform that maps a rectangle to the given bounds from
    // (0,0) to (max_value,max_value), inclusive.
    static Transform16 ToBounds(fract16 max_value);
    static Transform16 ToBounds(const point_xy<fract16> &min,
                                const point_xy<fract16> &max,
                                fract16 rotation = 0);
    Transform16() = default;
    fract16 scale_x = 0xffff;
    fract16 scale_y = 0xffff;
    fract16 x_offset = 0;
    fract16 y_offset = 0;
    fract16 rotation = 0;

    point_xy<fract16> transform(const point_xy<fract16> &xy) const;
};

} // namespace fl