#include "fl/stdint.h"

#include "fl/draw_visitor.h"
#include "fl/leds.h"
#include "fl/raster_sparse.h"
#include "fl/tile2x2.h"

namespace fl {

void XYRasterU8Sparse::draw(const CRGB &color, const XYMap &xymap, CRGB *out) {
    XYDrawComposited visitor(color, xymap, out);
    draw(xymap, visitor);
}

void XYRasterU8Sparse::draw(const CRGB &color, Leds *leds) {
    draw(color, leds->xymap(), leds->rgb());
}

void XYRasterU8Sparse::drawGradient(const Gradient &gradient,
                                    const XYMap &xymap, CRGB *out) {
    XYDrawGradient visitor(gradient, xymap, out);
    draw(xymap, visitor);
}

void XYRasterU8Sparse::drawGradient(const Gradient &gradient, Leds *leds) {
    drawGradient(gradient, leds->xymap(), leds->rgb());
}

void XYRasterU8Sparse::rasterize(const span<const Tile2x2_u8> &tiles) {
    // Tile2x2_u8::Rasterize(tiles, this, mAbsoluteBoundsSet ? &mAbsoluteBounds
    // : nullptr);
    if (tiles.size() == 0) {
        FASTLED_WARN("Rasterize: no tiles");
        return;
    }
    const rect<u16> *optional_bounds =
        mAbsoluteBoundsSet ? nullptr : &mAbsoluteBounds;

    // Check if the bounds are set.
    // draw all now unconditionally.
    for (const auto &tile : tiles) {
        // Rasterize the tile.
        rasterize_internal(tile, optional_bounds);
    }
    return;
}

void XYRasterU8Sparse::rasterize_internal(const Tile2x2_u8 &tile,
                                          const rect<u16> *optional_bounds) {
    const vec2<u16> &origin = tile.origin();
    for (int x = 0; x < 2; ++x) {
        for (int y = 0; y < 2; ++y) {
            u8 value = tile.at(x, y);
            if (!value) {
                continue;
            }
            int xx = origin.x + x;
            int yy = origin.y + y;
            if (optional_bounds && !optional_bounds->contains(xx, yy)) {
                continue;
            }
            write(vec2<u16>(xx, yy), value);
        }
    }
}

} // namespace fl

// XYRasterSparse_CRGB implementation
void fl::XYRasterSparse_CRGB::draw(const XYMap &xymap, CRGB *out) {
    for (const auto &it : mSparseGrid) {
        auto pt = it.first;
        if (!xymap.has(pt.x, pt.y)) {
            continue;
        }
        u32 index = xymap(pt.x, pt.y);
        const CRGB &color = it.second;
        // Only draw non-black pixels (since black represents "no data")
        if (color.r != 0 || color.g != 0 || color.b != 0) {
            out[index] = color;
        }
    }
}

void fl::XYRasterSparse_CRGB::draw(Leds *leds) {
    draw(leds->xymap(), leds->rgb());
}
