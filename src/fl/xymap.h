#pragma once

#include <stdint.h>
#include <string.h>

#include "crgb.h"
#include "fl/clamp.h"
#include "fl/force_inline.h"
#include "fl/lut.h"
#include "fl/namespace.h"
#include "fl/ptr.h"
#include "fl/xmap.h" // Include xmap.h for LUT16

namespace fl {
class ScreenMap;

FASTLED_FORCE_INLINE uint16_t xy_serpentine(uint16_t x, uint16_t y,
                                            uint16_t width, uint16_t height) {
    (void)height;
    if (y & 1) // Even or odd row?
        // reverse every second line for a serpentine lled layout
        return (y + 1) * width - 1 - x;
    else
        return y * width + x;
}

FASTLED_FORCE_INLINE uint16_t xy_line_by_line(uint16_t x, uint16_t y,
                                              uint16_t width, uint16_t height) {
    (void)height;
    return y * width + x;
}

// typedef for xyMap function type
typedef uint16_t (*XYFunction)(uint16_t x, uint16_t y, uint16_t width,
                               uint16_t height);

// Maps x,y -> led index
//
// The common output led matrix you can buy on amazon is in a serpentine layout.
//
// XYMap allows you do to do graphic calculations on an LED layout as if it were
// a grid.
class XYMap {
  public:
    enum XyMapType { kSerpentine = 0, kLineByLine, kFunction, kLookUpTable };

    static XYMap constructWithUserFunction(uint16_t width, uint16_t height,
                                           XYFunction xyFunction,
                                           uint16_t offset = 0);

    static XYMap constructRectangularGrid(uint16_t width, uint16_t height,
                                          uint16_t offset = 0);

    static XYMap constructWithLookUpTable(uint16_t width, uint16_t height,
                                          const uint16_t *lookUpTable,
                                          uint16_t offset = 0);

    static XYMap constructSerpentine(uint16_t width, uint16_t height,
                                     uint16_t offset = 0);

    static XYMap identity(uint16_t width, uint16_t height) {
        return constructRectangularGrid(width, height);
    }

    // is_serpentine is true by default. You probably want this unless you are
    // using a different layout
    XYMap(uint16_t width, uint16_t height, bool is_serpentine = true,
          uint16_t offset = 0);

    XYMap(const XYMap &other) = default;
    XYMap &operator=(const XYMap &other) = default;

    fl::ScreenMap toScreenMap() const;

    void mapPixels(const CRGB *input, CRGB *output) const;

    void convertToLookUpTable();

    void setRectangularGrid();

    uint16_t operator()(uint16_t x, uint16_t y) const {
        return mapToIndex(x, y);
    }

    uint16_t mapToIndex(const uint16_t &x, const uint16_t &y) const;

    template <typename IntType,
              typename = fl::enable_if_t<!fl::is_integral<IntType>::value>>
    uint16_t mapToIndex(IntType x, IntType y) const {
        x = fl::clamp<int>(x, 0, width - 1);
        y = fl::clamp<int>(y, 0, height - 1);
        return mapToIndex((uint16_t)x, (uint16_t)y);
    }

    bool has(uint16_t x, uint16_t y) const {
        return (x < width) && (y < height);
    }

    bool has(int x, int y) const {
        return (x >= 0) && (y >= 0) && has((uint16_t)x, (uint16_t)y);
    }

    bool isSerpentine() const { return type == kSerpentine; }
    bool isLineByLine() const { return type == kLineByLine; }
    bool isFunction() const { return type == kFunction; }
    bool isLUT() const { return type == kLookUpTable; }
    bool isRectangularGrid() const { return type == kLineByLine; }
    bool isSerpentineOrLineByLine() const {
        return type == kSerpentine || type == kLineByLine;
    }

    uint16_t getWidth() const;
    uint16_t getHeight() const;
    uint16_t getTotal() const;
    XyMapType getType() const;

  private:
    XYMap(uint16_t width, uint16_t height, XyMapType type);

    XyMapType type;
    uint16_t width;
    uint16_t height;
    XYFunction xyFunction = nullptr;
    fl::LUT16Ptr mLookUpTable; // optional refptr to look up table.
    uint16_t mOffset = 0;      // offset to be added to the output
};

} // namespace fl
