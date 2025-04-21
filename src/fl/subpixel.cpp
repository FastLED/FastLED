#include "subpixel.h"
#include "crgb.h"
#include "fl/xymap.h"
#include "fl/warn.h"

using namespace fl;

namespace fl {


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