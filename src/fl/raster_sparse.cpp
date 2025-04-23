

#include <stdint.h>

#include "fl/draw_visitor.h"
#include "fl/raster_sparse.h"
#include "fl/tile2x2.h"

namespace fl {

void XYRasterSparse::draw(const CRGB &color, const XYMap &xymap,
                          CRGB *out) {
    XYDrawComposited visitor(color, xymap, out);
    draw(xymap, visitor);
}

void XYRasterSparse::rasterize(const Slice<const Tile2x2_u8> &tiles) {
    // Tile2x2_u8::Rasterize(tiles, this, mAbsoluteBoundsSet ? &mAbsoluteBounds
    // : nullptr);
    if (tiles.size() == 0) {
        FASTLED_WARN("Rasterize: no tiles");
        return;
    }
    const rect_xy<int> *optional_bounds =
        mAbsoluteBoundsSet ? nullptr : &mAbsoluteBounds;


    // Check if the bounds are set.
    // draw all now unconditionally.
    for (const auto &tile : tiles) {
        // Rasterize the tile.
        rasterize_internal(tile, optional_bounds);
    }
    return;


}

void XYRasterSparse::rasterize_internal(const Tile2x2_u8 &tile,
                                        const rect_xy<int> *optional_bounds) {
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
            Pair<bool, uint8_t> entry = at(xx, yy);
            if (!entry.first) {
                // No value yet.
                write(point_xy<int>(xx, yy), value);
                continue;
            }
            // Already has a value.
            if (value > entry.second) {
                write(point_xy<int>(xx, yy), value);
            }
        }
    }
}

} // namespace fl