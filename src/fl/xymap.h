#pragma once

#include "fl/int.h"
#include "fl/namespace.h"
#include "fl/force_inline.h"
#include "fl/pair.h"
#include <string.h>

#include "crgb.h"
#include "fl/clamp.h"
#include "fl/lut.h"
#include "fl/memory.h"
#include "fl/deprecated.h"
#include "fl/xmap.h" // Include xmap.h for LUT16

namespace fl {
class ScreenMap;

FASTLED_FORCE_INLINE u16 xy_serpentine(u16 x, u16 y,
                                            u16 width, u16 height) {
    (void)height;
    if (y & 1) // Even or odd row?
        // reverse every second line for a serpentine lled layout
        return (y + 1) * width - 1 - x;
    else
        return y * width + x;
}

FASTLED_FORCE_INLINE u16 xy_line_by_line(u16 x, u16 y,
                                              u16 width, u16 height) {
    (void)height;
    return y * width + x;
}

// typedef for xyMap function type
typedef u16 (*XYFunction)(u16 x, u16 y, u16 width,
                               u16 height);

// Maps x,y -> led index
//
// The common output led matrix you can buy on amazon is in a serpentine layout.
//
// XYMap allows you do to do graphic calculations on an LED layout as if it were
// a grid.
class XYMap {
  public:
    enum XyMapType { kSerpentine = 0, kLineByLine, kFunction, kLookUpTable };

    static XYMap constructWithUserFunction(u16 width, u16 height,
                                           XYFunction xyFunction,
                                           u16 offset = 0);

    static XYMap constructRectangularGrid(u16 width, u16 height,
                                          u16 offset = 0);

    // This isn't working right, but the user function works just fine. The discussion
    // is here:
    // https://www.reddit.com/r/FastLED/comments/1kwwvcd/using_screenmap_with_nonstandard_led_layouts/
    // Remember that this is open source software so if you want to fix it, go for it.
    static XYMap constructWithLookUpTable(u16 width, u16 height,
                                          const u16 *lookUpTable,
                                          u16 offset = 0);

    static XYMap constructSerpentine(u16 width, u16 height,
                                     u16 offset = 0);

    static XYMap identity(u16 width, u16 height) {
        return constructRectangularGrid(width, height);
    }

    // is_serpentine is true by default. You probably want this unless you are
    // using a different layout
    XYMap(u16 width, u16 height, bool is_serpentine = true,
          u16 offset = 0);

    XYMap(const XYMap &other) = default;
    XYMap &operator=(const XYMap &other) = default;

    fl::ScreenMap toScreenMap() const;

    void mapPixels(const CRGB *input, CRGB *output) const;

    void convertToLookUpTable();

    void setRectangularGrid();

    u16 operator()(u16 x, u16 y) const {
        return mapToIndex(x, y);
    }

    u16 mapToIndex(const u16 &x, const u16 &y) const;

    template <typename IntType,
              typename = fl::enable_if_t<!fl::is_integral<IntType>::value>>
    u16 mapToIndex(IntType x, IntType y) const {
        x = fl::clamp<int>(x, 0, width - 1);
        y = fl::clamp<int>(y, 0, height - 1);
        return mapToIndex((u16)x, (u16)y);
    }

    bool has(u16 x, u16 y) const {
        return (x < width) && (y < height);
    }

    bool has(int x, int y) const {
        return (x >= 0) && (y >= 0) && has((u16)x, (u16)y);
    }

    bool isSerpentine() const { return type == kSerpentine; }
    bool isLineByLine() const { return type == kLineByLine; }
    bool isFunction() const { return type == kFunction; }
    bool isLUT() const { return type == kLookUpTable; }
    bool isRectangularGrid() const { return type == kLineByLine; }
    bool isSerpentineOrLineByLine() const {
        return type == kSerpentine || type == kLineByLine;
    }

    u16 getWidth() const;
    u16 getHeight() const;
    u16 getTotal() const;
    XyMapType getType() const;

  private:
    XYMap(u16 width, u16 height, XyMapType type);

    XyMapType type;
    u16 width;
    u16 height;
    XYFunction xyFunction = nullptr;
    fl::LUT16Ptr mLookUpTable; // optional refptr to look up table.
    u16 mOffset = 0;      // offset to be added to the output
};

} // namespace fl
