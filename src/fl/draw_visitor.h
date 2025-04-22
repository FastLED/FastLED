#pragma once

#include <stdint.h>
#include "fl/point.h"
#include "fl/namespace.h"
#include "fl/xymap.h"
#include "crgb.h"


namespace fl {

// A visitor interface for drawing uint8_t values.
struct XYDrawUint8Visitor {
    virtual ~XYDrawUint8Visitor() = default;
    virtual void draw(const point_xy<uint16_t> &pt, uint32_t index, uint8_t value) = 0;
};

struct XYDrawComposited: public XYDrawUint8Visitor {
    XYDrawComposited(const CRGB &color, const XYMap &xymap, CRGB *out);

    void draw(const point_xy<uint16_t> &pt, uint32_t index, uint8_t value) override ;
    const CRGB mColor;
    const XYMap mXYMap;
    CRGB *mOut;
};


}  // namespace fl

