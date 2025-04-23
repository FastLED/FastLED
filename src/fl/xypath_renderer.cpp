

#include <math.h>

#include "fl/xypath_renderer.h"
#include "fl/assert.h"
#include "fl/warn.h"
#include "fl/xypath.h"

namespace fl {

namespace {
uint8_t to_uint8(float f) {
    // convert to [0..255] range
    uint8_t i = static_cast<uint8_t>(f * 255.0f + .5f);
    return MIN(i, 255);
}
} // namespace

XYPathRenderer::XYPathRenderer(XYPathGeneratorPtr path,
                               TransformFloat transform)
    : mPath(path), mTransform(transform) {}

point_xy_float XYPathRenderer::compute_float(float alpha,
                                             const TransformFloat &tx) {
    point_xy_float xy = mPath->compute(alpha);
    point_xy_float out = tx.transform(xy);
    out = mGridTransform.transform(out);
    return out;
}

Tile2x2_u8 XYPathRenderer::at_subpixel(float alpha) {
    // 1) continuous point, in “pixel‐centers” coordinates [0.5 … W–0.5]
    if (!mDrawBoundsSet) {
        FASTLED_WARN("XYPathRenderer::at_subpixel: draw bounds not set");
        return Tile2x2_u8();
    }
    point_xy_float xy = at(alpha);

    // 2) shift back so whole‐pixels go 0…W–1, 0…H–1
    float x = xy.x - 0.5f;
    float y = xy.y - 0.5f;

    // 3) integer cell indices
    int cx = static_cast<int>(floorf(x));
    int cy = static_cast<int>(floorf(y));

    // 4) fractional offsets in [0..1)
    float fx = x - cx;
    float fy = y - cy;

    // 5) bilinear weights
    float w_ll = (1 - fx) * (1 - fy); // lower‑left
    float w_lr = fx * (1 - fy);       // lower‑right
    float w_ul = (1 - fx) * fy;       // upper‑left
    float w_ur = fx * fy;             // upper‑right

    // 6) build Tile2x2_u8 anchored at (cx,cy)
    Tile2x2_u8 out(point_xy<int>(cx, cy));
    out.lower_left() = to_uint8(w_ll);
    out.lower_right() = to_uint8(w_lr);
    out.upper_left() = to_uint8(w_ul);
    out.upper_right() = to_uint8(w_ur);

    return out;
}

} // namespace fl
