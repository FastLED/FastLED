#include "subpixel.h"
#include "crgb.h"
#include "fl/draw_visitor.h"
#include "fl/raster.h"
#include "fl/unused.h"
#include "fl/warn.h"
#include "fl/xymap.h"

using namespace fl;

namespace fl {

void SubPixel2x2::Rasterize(const Slice<const SubPixel2x2> &tiles,
                            XYRaster *out_raster, rect_xy<int> *optional_bounds) {
    if (tiles.size() == 0) {
        FASTLED_WARN("Rasterize: no tiles");
        return;
    }
    if (!out_raster) {
        FASTLED_WARN("Rasterize: no output raster");
        return;
    }


    rect_xy<int> bounds = tiles[0].bounds();
    //bool first = true;
    // auto origin = out_raster->origin();
    for (uint16_t i = 1; i < tiles.size(); ++i) {
        // we have to test if it's within bounds.
        const auto &tile = tiles[i];
        bounds.expand(tile.bounds());
    }

    // Optionally adjust the bounds.
    // rect_xy<int> *optional_bounds = nullptr;
    if (optional_bounds) {
        auto _min = bounds.mMin.getMax(optional_bounds->mMin);
        auto _max = bounds.mMax.getMin(optional_bounds->mMax);
        bounds = rect_xy<int>(_min, _max);        
    }

    // Won't reset the mMinMax bounds if this was set.
    out_raster->reset(bounds.mMin, bounds.width(), bounds.height());
    auto global_origin = out_raster->global_min();

    for (const auto &tile : tiles) {
        const point_xy<int> &origin = tile.origin();
        const point_xy<int> translate = origin - global_origin;
        for (int x = 0; x < 2; ++x) {
            for (int y = 0; y < 2; ++y) {
                uint8_t value = tile.at(x, y);
                int xx = translate.x + x;
                int yy = translate.y + y;
                auto &pt = out_raster->at(xx, yy);
                if (value > pt) {
                    pt = value;
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

}  // namespace fl
