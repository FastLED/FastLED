#include "subpixel.h"
#include "crgb.h"
#include "fl/draw_visitor.h"
#include "fl/raster.h"
#include "fl/raster_sparse.h"
#include "fl/unused.h"
#include "fl/warn.h"
#include "fl/xymap.h"

using namespace fl;

namespace fl {



void SubPixel2x2::Rasterize(const Slice<const SubPixel2x2> &tiles,
                            XYRasterSparse *out_raster,
                            rect_xy<int> *optional_bounds) {
    if (tiles.size() == 0) {
        FASTLED_WARN("Rasterize: no tiles");
        return;
    }
    if (!out_raster) {
        FASTLED_WARN("Rasterize: no output raster");
        return;
    }
    // Won't reset the mMinMax bounds if this was set.
    out_raster->reset();
    for (const auto &tile : tiles) {
        const point_xy<int> &origin = tile.origin();
        for (int x = 0; x < 2; ++x) {
            for (int y = 0; y < 2; ++y) {
                uint8_t value = tile.at(x, y);
                int xx = origin.x + x;
                int yy = origin.y + y;
                if (optional_bounds && !optional_bounds->contains(xx, yy)) {
                    continue;
                }
                fl::Pair<bool, uint8_t> entry = out_raster->at(xx, yy);
                if (!entry.first) {
                    // No value yet.
                    out_raster->add(point_xy<int>(xx, yy), value);
                    continue;
                }
                // Already has a value.
                if (value > entry.second) {
                    out_raster->add(point_xy<int>(xx, yy), value);
                }
            }
        }
    }
}

void SubPixel2x2::draw(const CRGB &color, const XYMap &xymap, CRGB *out) const {
    XYDrawComposited visitor(color, xymap, out);
    draw(xymap, visitor);
}

void SubPixel2x2::scale(uint8_t scale) {
    // scale the tile
    if (scale == 255) {
        return;
    }
    for (int x = 0; x < 2; ++x) {
        for (int y = 0; y < 2; ++y) {
            uint16_t value = at(x, y);
            at(x, y) = (value * scale) >> 8;
        }
    }
}



} // namespace fl
