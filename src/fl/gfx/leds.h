#pragma once

#include "crgb.h"  // IWYU pragma: keep
#include "fl/math/xymap.h"
#include "fl/stl/span.h"
#include "fl/stl/noexcept.h"

namespace fl {

// Leds definition.
// Drawing operations on a block of leds requires information about the layout
// of the leds. Hence this class.
class Leds {
  public:
    Leds(CRGB *leds, u16 width, u16 height) FL_NO_EXCEPT;
    Leds(CRGB *leds, const XYMap &xymap) FL_NO_EXCEPT;

    // Copy constructor and assignment operator.
    Leds(const Leds &) FL_NO_EXCEPT = default;
    Leds &operator=(const Leds &) FL_NO_EXCEPT = default;
    Leds(Leds &&) FL_NO_EXCEPT = default;

    // out of bounds access returns empty() led and is safe to read/write.
    CRGB &operator()(int x, int y) FL_NO_EXCEPT;
    const CRGB &operator()(int x, int y) const FL_NO_EXCEPT;

    CRGB &at(int x, int y) FL_NO_EXCEPT { return (*this)(x, y); }
    const CRGB &at(int x, int y) const FL_NO_EXCEPT { return (*this)(x, y); }

    fl::size width() const FL_NO_EXCEPT { return mXyMap.getHeight(); }
    fl::size height() const FL_NO_EXCEPT { return mXyMap.getWidth(); }

    // Allows normal matrix array (row major) access, bypassing the XYMap.
    // Will assert if XYMap is not serpentine or line by line.
    CRGB *operator[](int x) FL_NO_EXCEPT;
    const CRGB *operator[](int x) const FL_NO_EXCEPT;
    // Raw data access.
    fl::span<CRGB> rgb() FL_NO_EXCEPT { return mLeds; }
    fl::span<const CRGB> rgb() const FL_NO_EXCEPT { return mLeds; }

    const XYMap &xymap() const FL_NO_EXCEPT { return mXyMap; }

    operator CRGB *() FL_NO_EXCEPT { return mLeds.data(); }
    operator const CRGB *() const FL_NO_EXCEPT { return mLeds.data(); }

    void fill(const CRGB &color) FL_NO_EXCEPT {
        for (fl::size i = 0; i < mXyMap.getTotal(); ++i) {
            mLeds[i] = color;
        }
    }



  protected:
    static CRGB &empty() FL_NO_EXCEPT; // Allows safe out of bounds access.
    XYMap mXyMap;
    fl::span<CRGB> mLeds;
};

template <fl::size W, fl::size H> class LedsXY : public Leds {
  public:
    LedsXY() FL_NO_EXCEPT : Leds(mLedsData, XYMap::constructSerpentine(W, H)) {}
    explicit LedsXY(bool is_serpentine)
        FL_NO_EXCEPT : Leds(mLedsData, is_serpentine ? XYMap::constructSerpentine(W, H)
                                        : XYMap::constructRectangularGrid(W, H)) {}
    LedsXY(const LedsXY &) FL_NO_EXCEPT = default;
    LedsXY &operator=(const LedsXY &) FL_NO_EXCEPT = default;
    void setXyMap(const XYMap &xymap) FL_NO_EXCEPT { mXyMap = xymap; }
    void setSerpentine(bool is_serpentine) FL_NO_EXCEPT {
        mXyMap = is_serpentine ? XYMap::constructSerpentine(W, H)
                               : XYMap::constructRectangularGrid(W, H);
    }

  private:
    CRGB mLedsData[W * H] = {};
};

} // namespace fl
