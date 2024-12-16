#pragma once

#include <stdint.h>

#include "fl/namespace.h"
#include "fl/xmap.h"
#include "fx/fx.h"

namespace fl {

// Abstract base class for 1D effects that use a strip of LEDs.
class Fx1d : public Fx {
  public:
    Fx1d(uint16_t numLeds): Fx(numLeds), mXMap(numLeds, false) {}
    void setXmap(const XMap& xMap) {
      mXMap = xMap;
    }

    uint16_t xyMap(uint16_t x) const {
        return mXMap.mapToIndex(x);
    }

protected:
    XMap mXMap;
};

}  // namespace fl

