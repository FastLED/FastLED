#pragma once

#include <stdint.h
#include "xy.h"
#include "namespace.h"

FASTLED_NAMESPACE_BEGIN

class Fx2d {
  public:

    Fx2d(const XYMap& xyMap): mXyMap(xyMap) {}
    virtual ~Fx2d() {}
    virtual void lazyInit() {}
    virtual void draw() = 0;

    virtual const char* fxName() const = 0;  // Get the name of the current fx. This is the class name if there is only one.
    // Optionally implement these for multi fx classes.
    virtual int fxNum() const { return 1; };  // Return 1 if you only have one fx managed by this class.
    virtual void fxSet(int fx) {};  // Get the current fx number.
    virtual void fxNext(int fx = 1) {};  // Negative numbers are allowed. -1 means previous fx.
    virtual int fxGet() const { return 0; };  // Set the current fx number.

    uint16_t xy(uint16_t x, uint16_t y) const {
        return mXyMap.get(x, y);
    }
    uint16_t getHeight() const { return mXyMap.getHeight(); }
    uint16_t getWidth() const { return mXyMap.getWidth(); }
protected:
    XYMap mXyMap;
};

FASTLED_NAMESPACE_END