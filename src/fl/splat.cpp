
#include "fl/tile2x2.h"
#include "fl/splat.h"
#include "fl/math.h"

namespace fl {

static u8 to_uint8(float f) {
    // convert to [0..255] range
    u8 i = static_cast<u8>(f * 255.0f + .5f);
    return MIN(i, 255);
}

Tile2x2_u8 splat(vec2f xy) {
    // 1) collect values.
    float x = xy.x;
    float y = xy.y;

    // 2) integer cell indices
    i16 cx = static_cast<i16>(floorf(x));
    i16 cy = static_cast<i16>(floorf(y));

    // 3) fractional offsets in [0..1)
    float fx = x - cx;
    float fy = y - cy;

    // 4) bilinear weights
    float w_ll = (1 - fx) * (1 - fy); // lower‑left
    float w_lr = fx * (1 - fy);       // lower‑right
    float w_ul = (1 - fx) * fy;       // upper‑left
    float w_ur = fx * fy;             // upper‑right

    // 5) build Tile2x2_u8 anchored at (cx,cy)
    Tile2x2_u8 out(vec2<u16>(cx, cy));
    out.lower_left() = to_uint8(w_ll);
    out.lower_right() = to_uint8(w_lr);
    out.upper_left() = to_uint8(w_ul);
    out.upper_right() = to_uint8(w_ur);

    return out;
}

}  // namespace
