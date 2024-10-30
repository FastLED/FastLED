#pragma once

#include <stdint.h>

#include "namespace.h"
#include "xmap.h"
#include "fx/fx.h"

FASTLED_NAMESPACE_BEGIN

// Abstract base class for 1D effects that use a strip of LEDs.
class FxStrip : public Fx {
  public:
    FxStrip(uint16_t numLeds): Fx(numLeds), mXMap(numLeds, false) {}
    void setXmap(const XMap& xMap) {
      mXMap = xMap;
    }

    uint16_t xyMap(uint16_t x) const {
        return mXMap.mapToIndex(x);
    }

protected:
    XMap mXMap;
};

FASTLED_NAMESPACE_END

