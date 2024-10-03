#pragma once

#include <stdint.h>
#include "namespace.h"
#include "fx/_xmap.h"

FASTLED_NAMESPACE_BEGIN

// Abstract base class for 1D effects that use a strip of LEDs.
class FxStrip {
  public:
    FxStrip(uint16_t numLeds): mNumLeds(numLeds), mXMap(numLeds, false) {}
    virtual ~FxStrip() {}
    virtual void lazyInit() {}
    virtual void draw() = 0;

    virtual const char* fxName() const = 0;  // Get the name of the current fx. This is the class name if there is only one.
    // Optionally implement these for multi fx classes.
    virtual int fxNum() const { return 1; };  // Return 1 if you only have one fx managed by this class.
    virtual void fxSet(int fx) {};  // Set the current fx number.
    virtual void fxNext(int fx = 1) {};  // Negative numbers are allowed. -1 means previous fx.
    virtual int fxGet() const { return 0; };  // Get the current fx number.

    uint16_t getNumLeds() const { return mNumLeds; }
    void setXmap(const XMap& xMap) { mXMap = xMap; }

protected:
    uint16_t mNumLeds;
    XMap mXMap;
};

FASTLED_NAMESPACE_END

