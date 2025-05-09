#pragma once

#include "crgb.h"
#include "fl/xymap.h"

namespace fl {

class LedGrid {
  public:
    LedGrid(CRGB* leds, uint16_t width, uint16_t height);
    LedGrid(CRGB *leds, const XYMap &xymap);

    // Copy constructor and assignment operator.
    LedGrid(const LedGrid &) = default;
    LedGrid &operator=(const LedGrid &) = default;
    LedGrid(LedGrid &&) = default;

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

  protected:
    static CRGB &empty(); // Allows safe out of bounds access.
    const XYMap mXyMap;
    CRGB *mLeds;
};

template <size_t W, size_t H> class LedsXY : public LedGrid {
  public:
    LedsXY(): LedGrid(mLeds, XYMap::constructSerpentine(W, H)) {}
    explicit LedsXY(bool is_serpentine)
        : LedGrid(mLeds, is_serpentine ? XYMap::constructSerpentine(W, H)
                                       : XYMap::constructRectangularGrid(W, H)) {
    }
    LedsXY(const LedsXY &) = default;
    LedsXY &operator=(const LedsXY &) = default;
    void setXyMap(const XYMap &xymap) {
        mXyMap = xymap;
    }
    void setSerpentine(bool is_serpentine) {
        mXyMap = is_serpentine ? XYMap::constructSerpentine(W, H)
                               : XYMap::constructRectangularGrid(W, H);
    }

  private:
    static_assert(W > 0 && H > 0, "Width and height must be greater than 0");
    static_assert(W * H < 65536, "Width and height must be less than 65536");
    static_assert(W % 2 == 0, "Width must be even");
    static_assert(H % 2 == 0, "Height must be even");
    CRGB mLeds[W * H] = {};
};

} // namespace fl
