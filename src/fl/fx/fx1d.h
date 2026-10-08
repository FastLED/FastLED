#pragma once

#include "fl/stl/int.h"
#include "fl/math/xmap.h"
#include "fl/fx/fx.h"
#include "fl/stl/noexcept.h"

namespace fl {

// Abstract base class for 1D effects that use a strip of LEDs.
class Fx1d : public Fx {
  public:
    Fx1d(u16 numLeds) FL_NO_EXCEPT : Fx(numLeds), mXMap(numLeds, false) {}
    void setXmap(const XMap &xMap) FL_NO_EXCEPT { mXMap = xMap; }

    u16 xyMap(u16 x) const FL_NO_EXCEPT { return mXMap.mapToIndex(x); }

  protected:
    XMap mXMap;
};

} // namespace fl
