
#include <stdint.h>
#include "fl/point.h"
#include "fl/draw_visitor.h"
#include "fl/unused.h"

namespace fl {

XYDrawComposited::XYDrawComposited(const CRGB &color, const XYMap &xymap, CRGB *out)
    : mColor(color), mXYMap(xymap), mOut(out) {}

void XYDrawComposited::draw(const point_xy<int> &pt, uint32_t index, uint8_t value) {
    FASTLED_UNUSED(pt);
    CRGB& c = mOut[index];
    CRGB blended = mColor;
    blended.fadeToBlackBy(255 - value);
    c = CRGB::blendAlphaMaxChannel(blended, c);
}

}  // namespace fl

