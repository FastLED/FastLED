
#include <stdint.h>

#include "crgb.h"
#include "fl/draw_visitor.h"
#include "fl/grid.h"
#include "fl/namespace.h"
#include "fl/point.h"
#include "fl/raster.h"
#include "fl/subpixel.h"
#include "fl/xymap.h"
#include "fl/warn.h"

namespace fl {

void Raster::draw(const CRGB &color, const XYMap &xymap, CRGB *out) const {
    XYDrawComposited visitor(color, xymap, out);
    draw(xymap, &visitor);
}

void Raster::draw(const XYMap &xymap, XYDrawUint8Visitor *visitor) const {
    const uint16_t w = width();
    const uint16_t h = height();
    const point_xy<int> origin = this->origin();
    for (uint16_t x = 0; x < w; ++x) {
        for (uint16_t y = 0; y < h; ++y) {
            uint16_t xx = x + origin.x;
            uint16_t yy = y + origin.y;
            if (!xymap.has(xx, yy)) {
                continue;
            }
            uint32_t index = xymap(xx, yy);
            uint8_t value = at(x, y);
            if (value > 0) {
                point_xy<int> pt = {xx, yy};
                visitor->draw(pt, index, value);
            }
        }
    }
}

void Raster::rasterize(const Slice<const SubPixel2x2> &tiles) {    
    rect_xy<int> *optional_bounds  = nullptr;
    rect_xy<int> maybe_bounds;
    if (!mWidthHeight.is_zero()) {
        // optional_bounds = &mWidthHeight;
        maybe_bounds.mMin = point_xy<int>(0, 0);
        maybe_bounds.mMax = point_xy<int>(mWidthHeight.x, mWidthHeight.y);
        optional_bounds = &maybe_bounds;
    }
    SubPixel2x2::Rasterize(tiles, this, optional_bounds);
}

} // namespace fl