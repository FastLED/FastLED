

#include <stdint.h>

#include "fl/raster_sparse.h"
#include "fl/subpixel.h"

namespace fl {


void XYRasterSparse::rasterize(const Slice<const SubPixel2x2> &tiles) {
    rect_xy<int> *optional_bounds = nullptr;
    rect_xy<int> maybe_bounds;
    if (!mAbsoluteBounds.empty()) {
        // optional_bounds = &mWidthHeight;
        maybe_bounds.mMin = point_xy<int>(0, 0);
        maybe_bounds.mMax = point_xy<int>(mBounds.width(), mBounds.height());
        optional_bounds = &maybe_bounds;
    }
    SubPixel2x2::Rasterize(tiles, this, optional_bounds);

}


} // namespace fl