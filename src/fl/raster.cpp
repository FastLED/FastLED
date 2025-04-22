
#include <stdint.h>

#include "fl/point.h"
#include "fl/grid.h"
#include "fl/namespace.h"
#include "fl/raster.h"
#include "fl/xymap.h"
#include "crgb.h"


namespace fl {

void Raster::draw(const CRGB &color, const XYMap &xymap, CRGB *out) const {
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
                int index = xymap(xx, yy);
                CRGB& c = out[index];
                CRGB blended = color;
                blended.fadeToBlackBy(255 - value);
                c = CRGB::blendAlphaMaxChannel(blended, c);
            }
        }
    }

}

} // namespace fl