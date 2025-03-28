#pragma once

#include <stdint.h>
#include <string.h>

#include "crgb.h"
#include "fl/force_inline.h"
#include "fl/lut.h"
#include "fl/ptr.h"
#include "fl/xmap.h" // Include xmap.h for LUT16
#include "fl/namespace.h"

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

// XYMap holds either a function or a look up table to map x, y coordinates to a
// 1D index.
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

    uint16_t mapToIndex(uint16_t x, uint16_t y) const;
    uint16_t mapToIndex(int x, int y) const {
        if (x < 0) { x = 0; }
        else if (uint16_t(x) >= width) { x = width - 1; }
        if (y < 0) { y = 0; }
        else if (uint16_t(y) >= height) { y = height - 1; }
        return mapToIndex((uint16_t)x, (uint16_t)y);
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
