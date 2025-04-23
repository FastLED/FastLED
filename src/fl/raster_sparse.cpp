

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
    SubPixel2x2::Rasterize(tiles, this, mAbsoluteBoundsSet ? &mAbsoluteBounds : nullptr);
}

} // namespace fl