#pragma once

#include "fl/stdint.h"

#include "fl/int.h"
#include "fl/namespace.h"
#include "fl/xmap.h"
#include "fx/fx.h"

namespace fl {

// Abstract base class for 1D effects that use a strip of LEDs.
class Fx1d : public Fx {
  public:
    Fx1d(u16 numLeds) : Fx(numLeds), mXMap(numLeds, false) {}
    void setXmap(const XMap &xMap) { mXMap = xMap; }

    u16 xyMap(u16 x) const { return mXMap.mapToIndex(x); }

  protected:
    XMap mXMap;
};

} // namespace fl
