#include "subpixel.h"
#include "crgb.h"
#include "fl/xymap.h"
#include "fl/warn.h"
#include "fl/raster.h"

using namespace fl;

namespace fl {


void SubPixel2x2::DrawRaster(const Slice<SubPixel2x2> &tiles, Raster* out_raster) {
    uint16_t min_left = 0xffff;
    uint16_t max_top = 0;
    uint16_t max_right = 0xffff;
    uint16_t min_bottom = 0;

    for (const auto& tile : tiles) {
        const point_xy<uint16_t> &origin = tile.origin();
        min_left = MIN(min_left, origin.x);
        max_top = MAX(max_top, origin.y);
        max_right = MIN(max_right, origin.x);
        min_bottom = MIN(min_bottom, origin.y);
    }

    uint16_t width = max_right - min_left;
    uint16_t height = max_top - min_bottom;
    point_xy<uint16_t> global_origin(min_left, max_top);

    out_raster->reset(global_origin, width+1, height+1);

    for (const auto& tile : tiles) {
        const point_xy<uint16_t> &origin = tile.origin();
        const point_xy<uint16_t> translate = origin - global_origin;
        for (int x = 0; x < 2; ++x) {
            for (int y = 0; y < 2; ++y) {
                uint8_t value = tile.at(x, y);
                int xx = translate.x + x;
                int yy = translate.y + y;
                auto& pt = out_raster->at(xx, yy);
                if (value > pt) {
                    pt = value;
                }
            }
        }
    }
}

void SubPixel2x2::draw(const CRGB &color, const XYMap &xymap, CRGB *out) const {
    uint8_t ll = at(0,0);
    uint8_t ul = at(0,1);
    uint8_t lr = at(1,0);
    uint8_t ur = at(1,1);
    if (ll > 0) {
        int x = mOrigin.x;
        int y = mOrigin.y;
        if (xymap.has(x, y)) {
            int index = xymap(x, y);
            CRGB& c = out[index];
            CRGB blended = color;
            blended.fadeToBlackBy(255 - ll);
            c = CRGB::blendAlphaMaxChannel(blended, c);
        }
    }

    if (ul > 0) {
        int x = mOrigin.x;
        int y = mOrigin.y + 1;
        if (xymap.has(x, y)) {
            int index = xymap(x, y);
            CRGB& c = out[index];
            CRGB blended = color;
            blended.fadeToBlackBy(255 - ul);
            c = CRGB::blendAlphaMaxChannel(blended, c);
            // FASTLED_WARN("wrote upper left: " << c.toString());
        }
    }

    if (lr > 0) {
        int x = mOrigin.x + 1;
        int y = mOrigin.y;
        if (xymap.has(x, y)) {
            int index = xymap(x, y);
            CRGB& c = out[index];
            CRGB blended = color;
            blended.fadeToBlackBy(255 - lr);
            c = CRGB::blendAlphaMaxChannel(blended, c);
            // FASTLED_WARN("wrote lower right: " << c.toString());
        }
    }

    if (ur > 0) {
        int x = mOrigin.x + 1;
        int y = mOrigin.y + 1;
        if (xymap.has(x, y)) {
            int index = xymap(x, y);
            CRGB& c = out[index];
            CRGB blended = color;
            blended.fadeToBlackBy(255 - ur);
            c = CRGB::blendAlphaMaxChannel(blended, c);
            // FASTLED_WARN("wrote lower left: " << c.toString());
        }
    }
}

}