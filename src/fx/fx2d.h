#pragma once

#include <stdint.h>

#include "fl/xymap.h"
#include "fl/namespace.h"
#include "fx/fx.h"
#include "fl/ptr.h"

namespace fl {

FASTLED_SMART_PTR(Fx2d);

// Abstract base class for 2D effects that use a grid, which is defined
// by an XYMap.
class Fx2d : public Fx {
  public:
    // XYMap holds either a function or a look up table to map x, y coordinates to a 1D index.
    Fx2d(const XYMap& xyMap): Fx(xyMap.getTotal()), mXyMap(xyMap) {}
    uint16_t xyMap(uint16_t x, uint16_t y) const {
        return mXyMap.mapToIndex(x, y);
    }
    uint16_t getHeight() const { return mXyMap.getHeight(); }
    uint16_t getWidth() const { return mXyMap.getWidth(); }
    void setXYMap(const XYMap& xyMap) { mXyMap = xyMap; }
    XYMap& getXYMap() { return mXyMap; }
    const XYMap& getXYMap() const { return mXyMap; }
protected:
    XYMap mXyMap;
};

}  // namespace fl
