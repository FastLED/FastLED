
#include "fl/lut.h"

namespace fl {

struct TransformFloat {
    TransformFloat() = default;
    float scale_x = 1.0f;
    float scale_y = 1.0f;
    float x_offset = 0.0f;
    float y_offset = 0.0f;
    float rotation = 0.0f;
    pair_xy_float transform(const pair_xy_float &xy) const;
};

struct Transform16 {
    // Make a transform that maps a rectangle to the given bounds from
    // (0,0) to (max_value,max_value), inclusive.
    static Transform16 ToBounds(uint16_t max_value);
    static Transform16 ToBounds(const pair_xy<uint16_t> &min, 
                                const pair_xy<uint16_t> &max) ;
    Transform16() = default;
    uint16_t scale_x = 0xffff;
    uint16_t scale_y = 0xffff;
    uint16_t x_offset = 0;
    uint16_t y_offset = 0;
    uint16_t rotation = 0;

    pair_xy<uint16_t> transform(const pair_xy<uint16_t> &xy) const;
};

}