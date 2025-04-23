

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
    // SubPixel2x2::Rasterize(tiles, this, mAbsoluteBoundsSet ? &mAbsoluteBounds : nullptr);
    if (tiles.size() == 0) {
        FASTLED_WARN("Rasterize: no tiles");
        return;
    }
    const rect_xy<int> *optional_bounds = mAbsoluteBoundsSet ? nullptr : &mAbsoluteBounds;
    // Won't reset the mMinMax bounds if this was set.
    //out_raster->reset();
    for (const auto &tile : tiles) {
        const point_xy<int> &origin = tile.origin();
        for (int x = 0; x < 2; ++x) {
            for (int y = 0; y < 2; ++y) {
                uint8_t value = tile.at(x, y);
                if (!value) {
                    continue;
                }
                int xx = origin.x + x;
                int yy = origin.y + y;
                if (optional_bounds && !optional_bounds->contains(xx, yy)) {
                    continue;
                }
                fl::Pair<bool, uint8_t> entry = at(xx, yy);
                if (!entry.first) {
                    // No value yet.
                    add(point_xy<int>(xx, yy), value);
                    FASTLED_WARN("Adding value " << value << " at " << xx << "," << yy);
                    continue;
                }
                // Already has a value.
                if (value > entry.second) {
                    add(point_xy<int>(xx, yy), value);
                }
            }
        }
    }

}

} // namespace fl