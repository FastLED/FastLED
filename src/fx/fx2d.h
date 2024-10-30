#pragma once

#include <stdint.h>

#include "xymap.h"
#include "namespace.h"
#include "fx/fx.h"
#include "ref.h"

FASTLED_NAMESPACE_BEGIN

FASTLED_SMART_REF(FxGrid);

// Abstract base class for 2D effects that use a grid, which is defined
// by an XYMap.
class FxGrid : public Fx {
  public:
    // XYMap holds either a function or a look up table to map x, y coordinates to a 1D index.
    FxGrid(const XYMap& xyMap): Fx(xyMap.getTotal()), mXyMap(xyMap) {}
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

FASTLED_NAMESPACE_END
