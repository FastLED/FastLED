#pragma once

#include "fl/stl/stdint.h"

#include "fl/stl/shared_ptr.h"         // For FASTLED_SHARED_PTR macros
#include "fl/math/xymap.h"
#include "fl/fx/fx.h"
#include "fl/stl/noexcept.h"

namespace fl {

FASTLED_SHARED_PTR(Fx2d);

// Abstract base class for 2D effects that use a grid, which is defined
// by an XYMap.
class Fx2d : public Fx {
  public:
    // XYMap holds either a function or a look up table to map x, y coordinates
    // to a 1D index.
    Fx2d(const XYMap &xyMap) FL_NO_EXCEPT : Fx(xyMap.getTotal()), mXyMap(xyMap) {}
    u16 xyMap(u16 x, u16 y) const FL_NO_EXCEPT {
        return mXyMap.mapToIndex(x, y);
    }
    u16 getHeight() const FL_NO_EXCEPT { return mXyMap.getHeight(); }
    u16 getWidth() const FL_NO_EXCEPT { return mXyMap.getWidth(); }
    void setXYMap(const XYMap &xyMap) FL_NO_EXCEPT { mXyMap = xyMap; }
    XYMap &getXYMap() FL_NO_EXCEPT { return mXyMap; }
    const XYMap &getXYMap() const FL_NO_EXCEPT { return mXyMap; }

  protected:
    XYMap mXyMap;
};

} // namespace fl
