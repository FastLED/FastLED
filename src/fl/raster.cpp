
#include <stdint.h>

#include "fl/point.h"
#include "fl/grid.h"
#include "fl/namespace.h"
#include "fl/raster.h"
#include "fl/xymap.h"
#include "crgb.h"

namespace fl {

struct ApplyBlending: public DrawUint8Visitor {
    ApplyBlending(const CRGB &color, const XYMap &xymap, CRGB *out)
        : mColor(color), mXYMap(xymap), mOut(out) {}

    void draw(const point_xy<uint16_t> &pt, uint8_t value) override {
        int x = pt.x;
        int y = pt.y;
        if (mXYMap.has(x, y)) {
            int index = mXYMap(x, y);
            CRGB& c = mOut[index];
            CRGB blended = mColor;
            blended.fadeToBlackBy(255 - value);
            c = CRGB::blendAlphaMaxChannel(blended, c);
        }
    }
    const CRGB &mColor;
    const XYMap &mXYMap;
    CRGB *mOut;
};

void Raster::draw(const CRGB &color, const XYMap &xymap, CRGB *out) const {
    ApplyBlending visitor(color, xymap, out);
    draw(xymap, &visitor);
}

void Raster::draw(const XYMap& xymap, DrawUint8Visitor* visitor) const {
    const uint16_t w = width();
    const uint16_t h = height();
    const point_xy<uint16_t> origin = this->origin();
    for (uint16_t x = 0; x < w; ++x) {
        for (uint16_t y = 0; y < h; ++y) {
            uint16_t xx = x + origin.x;
            uint16_t yy = y + origin.y;
            if (!xymap.has(xx, yy)) {
                continue;
            }
            uint8_t value = at(x, y);
            if (value > 0) {
                point_xy<uint16_t> pt = {xx, yy};
                visitor->draw(pt, value);
            }
        }
    }
}

} // namespace fl