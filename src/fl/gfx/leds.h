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
    Leds(CRGB *leds, u16 width, u16 height);
    Leds(CRGB *leds, const XYMap &xymap);

    // Copy constructor and assignment operator.
    Leds(const Leds &) FL_NOEXCEPT = default;
    Leds &operator=(const Leds &) FL_NOEXCEPT = default;
    Leds(Leds &&) FL_NOEXCEPT = default;

    // out of bounds access returns empty() led and is safe to read/write.
    CRGB &operator()(int x, int y);
    const CRGB &operator()(int x, int y) const;

    CRGB &at(int x, int y) { return (*this)(x, y); }
    const CRGB &at(int x, int y) const { return (*this)(x, y); }

    fl::size width() const { return mXyMap.getHeight(); }
    fl::size height() const { return mXyMap.getWidth(); }

    // Allows normal matrix array (row major) access, bypassing the XYMap.
    // Will assert if XYMap is not serpentine or line by line.
    CRGB *operator[](int x);
    const CRGB *operator[](int x) const;
    // Raw data access.
    fl::span<CRGB> rgb() { return mLeds; }
    fl::span<const CRGB> rgb() const { return mLeds; }

    const XYMap &xymap() const { return mXyMap; }

    operator CRGB *() { return mLeds.data(); }
    operator const CRGB *() const { return mLeds.data(); }

    void fill(const CRGB &color) {
        for (fl::size i = 0; i < mXyMap.getTotal(); ++i) {
            mLeds[i] = color;
        }
    }



  protected:
    static CRGB &empty(); // Allows safe out of bounds access.
    XYMap mXyMap;
    fl::span<CRGB> mLeds;
};

template <fl::size W, fl::size H> class LedsXY : public Leds {
  public:
    LedsXY() FL_NOEXCEPT : Leds(mLedsData, XYMap::constructSerpentine(W, H)) {}
    explicit LedsXY(bool is_serpentine)
        : Leds(mLedsData, is_serpentine ? XYMap::constructSerpentine(W, H)
                                        : XYMap::constructRectangularGrid(W, H)) {}
    LedsXY(const LedsXY &) FL_NOEXCEPT = default;
    LedsXY &operator=(const LedsXY &) FL_NOEXCEPT = default;
    void setXyMap(const XYMap &xymap) { mXyMap = xymap; }
    void setSerpentine(bool is_serpentine) {
        mXyMap = is_serpentine ? XYMap::constructSerpentine(W, H)
                               : XYMap::constructRectangularGrid(W, H);
    }

  private:
    CRGB mLedsData[W * H] = {};
};

} // namespace fl
