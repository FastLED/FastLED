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
                            Raster *out_raster) {
    if (tiles.size() == 0) {
        FASTLED_WARN("Rasterize: no tiles");
        return;
    }
    if (!out_raster) {
        FASTLED_WARN("Rasterize: no output raster");
        return;
    }

    auto &first_tile = tiles[0];
    rect_xy<uint16_t> bounds = first_tile.bounds();

    for (uint16_t i = 1; i < tiles.size(); ++i) {
        const auto &tile = tiles[i];
        bounds.expand(tile.bounds());
    }

    out_raster->reset(bounds.mMin, bounds.width(), bounds.height());
    auto global_origin = out_raster->global_min();

    for (const auto &tile : tiles) {
        const point_xy<uint16_t> &origin = tile.origin();
        const point_xy<uint16_t> translate = origin - global_origin;
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
    draw(xymap, &visitor);
}

void SubPixel2x2::draw(const XYMap &xymap, XYDrawUint8Visitor *visitor) const {
    for (uint16_t x = 0; x < 2; ++x) {
        for (uint16_t y = 0; y < 2; ++y) {
            uint8_t value = at(x, y);
            if (value > 0) {
                int xx = mOrigin.x + x;
                int yy = mOrigin.y + y;
                if (xymap.has(xx, yy)) {
                    int index = xymap(xx, yy);
                    visitor->draw(point_xy<uint16_t>(xx, yy), index, value);
                }
            }
        }
    }
}

}  // namespace fl
