

#include <math.h>

#include "fl/assert.h"
#include "fl/warn.h"
#include "fl/xypath.h"
#include "fl/xypath_renderer.h"
#include "fl/splat.h"

namespace fl {

XYPathRenderer::XYPathRenderer(XYPathGeneratorPtr path,
                               TransformFloat transform)
    : mPath(path), mTransform(transform) {}

vec2f XYPathRenderer::compute_float(float alpha, const TransformFloat &tx) {
    vec2f xy = mPath->compute(alpha);
    vec2f out = tx.transform(xy);
    out = mGridTransform.transform(out);
    return out;
}

Tile2x2_u8 XYPathRenderer::at_subpixel(float alpha) {
    // 1) continuous point, in “pixel‐centers” coordinates [0.5 … W–0.5]
    if (!mDrawBoundsSet) {
        FASTLED_WARN("XYPathRenderer::at_subpixel: draw bounds not set");
        return Tile2x2_u8();
    }
    vec2f xy = at(alpha);

    // 1) shift back so whole‐pixels go 0…W–1, 0…H–1
    xy.x -= 0.5f;
    xy.y -= 0.5f;

    return splat(xy);
}

} // namespace fl
