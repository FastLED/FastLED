
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

void XYRaster::draw(const CRGB &color, const XYMap &xymap, CRGB *out) const {
    XYDrawComposited visitor(color, xymap, out);
    draw(xymap, visitor);
}


void XYRaster::rasterize(const Slice<const SubPixel2x2> &tiles) {    
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