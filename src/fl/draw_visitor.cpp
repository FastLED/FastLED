
#include <stdint.h>
#include "fl/point.h"
#include "fl/draw_visitor.h"

namespace fl {

DrawComposited::DrawComposited(const CRGB &color, const XYMap &xymap, CRGB *out)
    : mColor(color), mXYMap(xymap), mOut(out) {}

void DrawComposited::draw(const point_xy<uint16_t> &pt, uint8_t value) {
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

}  // namespace fl

