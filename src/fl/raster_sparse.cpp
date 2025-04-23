

#include <stdint.h>

#include "fl/draw_visitor.h"
#include "fl/raster_sparse.h"
#include "fl/subpixel.h"

namespace fl {

void XYRasterSparse::draw(const CRGB &color, const XYMap &xymap,
                          CRGB *out) const {
    XYDrawComposited visitor(color, xymap, out);
    draw(xymap, visitor);
}

void XYRasterSparse::rasterize(const Slice<const SubPixel2x2> &tiles) {
    rect_xy<int> *optional_bounds = nullptr;
    rect_xy<int> maybe_bounds;
    if (!mAbsoluteBounds.empty()) {
        optional_bounds = &mAbsoluteBounds;
    }
    SubPixel2x2::Rasterize(tiles, this, optional_bounds);
}

} // namespace fl