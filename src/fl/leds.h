#pragma once

#include "crgb.h"
#include "fl/xymap.h"

namespace fl {

// Leds definition.
// Drawing operations on a block of leds requires information about the layout
// of the leds. Hence this class.
class Leds {
  public:
    Leds(CRGB *leds, uint16_t width, uint16_t height);
    Leds(CRGB *leds, const XYMap &xymap);

    // Copy constructor and assignment operator.
    Leds(const Leds &) = default;
    Leds &operator=(const Leds &) = default;
    Leds(Leds &&) = default;

    // out of bounds access returns empty() led and is safe to read/write.
    CRGB &operator()(int x, int y);
    const CRGB &operator()(int x, int y) const;

    CRGB &at(int x, int y) { return (*this)(x, y); }
    const CRGB &at(int x, int y) const { return (*this)(x, y); }

    size_t width() const { return mXyMap.getHeight(); }
    size_t height() const { return mXyMap.getWidth(); }

    // Allows normal matrix array (row major) access, bypassing the XYMap.
    // Will assert if XYMap is not serpentine or line by line.
    CRGB *operator[](int x);
    const CRGB *operator[](int x) const;
    // Raw data access.
    CRGB *rgb() { return mLeds; }
    const CRGB *rgb() const { return mLeds; }

    const XYMap &xymap() const { return mXyMap; }

    operator CRGB *() { return mLeds; }
    operator const CRGB *() const { return mLeds; }

    void fill(const CRGB &color) {
        for (size_t i = 0; i < mXyMap.getTotal(); ++i) {
            mLeds[i] = color;
        }
    }

  protected:
    static CRGB &empty(); // Allows safe out of bounds access.
    XYMap mXyMap;
    CRGB *mLeds;
};

template <size_t W, size_t H> class LedsXY : public Leds {
  public:
    LedsXY() : Leds(mLeds, XYMap::constructSerpentine(W, H)) {}
    explicit LedsXY(bool is_serpentine)
        : Leds(mLeds, is_serpentine ? XYMap::constructSerpentine(W, H)
                                    : XYMap::constructRectangularGrid(W, H)) {}
    LedsXY(const LedsXY &) = default;
    LedsXY &operator=(const LedsXY &) = default;
    void setXyMap(const XYMap &xymap) { mXyMap = xymap; }
    void setSerpentine(bool is_serpentine) {
        mXyMap = is_serpentine ? XYMap::constructSerpentine(W, H)
                               : XYMap::constructRectangularGrid(W, H);
    }

  private:
    CRGB mLeds[W * H] = {};
};

} // namespace fl
