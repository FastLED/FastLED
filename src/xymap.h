#pragma once

#include "force_inline.h"
#include "ptr.h"
#include "xmap.h" // Include xmap.h for LUT16
#include <stdint.h>
#include <string.h>
#include "lut16.h"

FASTLED_FORCE_INLINE uint16_t xy_serpentine(uint16_t x, uint16_t y,
                                            uint16_t width, uint16_t height) {
    if (y & 1) // Even or odd row?
        // reverse every second line for a serpentine lled layout
        return (y + 1) * width - 1 - x;
    else
        return y * width + x;
}

FASTLED_FORCE_INLINE uint16_t xy_line_by_line(uint16_t x, uint16_t y,
                                              uint16_t width, uint16_t height) {
    return y * width + x;
}

// typedef for xyMap function type
typedef uint16_t (*XYFunction)(uint16_t x, uint16_t y, uint16_t width,
                               uint16_t height);

// XYMap holds either a function or a look up table to map x, y coordinates to a
// 1D index.
class XYMap {
  public:
    enum XyMapType { kSeperentine = 0, kLineByLine, kFunction, kLookUpTable };

    static XYMap constructWithUserFunction(uint16_t width, uint16_t height,
                                           XYFunction xyFunction) {
        XYMap out(width, height, kFunction);
        out.xyFunction = xyFunction;
        return out;
    }

    static XYMap constructRectangularGrid(uint16_t width, uint16_t height) {
        return XYMap(width, height, kLineByLine);
    }

    static XYMap constructWithLookUpTable(uint16_t width, uint16_t height,
                                          const uint16_t *lookUpTable) {
        XYMap out(width, height, kLookUpTable);
        out.mLookUpTable = LUT16Ptr::FromHeap(new LUT16(width * height));
        memcpy(out.mLookUpTable->getData(), lookUpTable,
               width * height * sizeof(uint16_t));
        return out;
    }

    // is_serpentine is true by default. You probably want this unless you are
    // using a different layout
    XYMap(uint16_t width, uint16_t height, bool is_serpentine = true)
        : type(is_serpentine ? kSeperentine : kLineByLine),
          width(width), height(height) {}

    XYMap(const XYMap &other)
        : type(other.type), width(other.width), height(other.height),
          xyFunction(other.xyFunction), mLookUpTable(other.mLookUpTable) {}

    void convertToLookUpTable() {
        if (type == kLookUpTable) {
            return;
        }
        mLookUpTable = LUT16Ptr::FromHeap(new LUT16(width * height));
        uint16_t *data = mLookUpTable->getData();
        for (uint16_t y = 0; y < height; y++) {
            for (uint16_t x = 0; x < width; x++) {
                data[y * width + x] = mapToIndex(x, y);
            }
        }
        type = kLookUpTable;
        xyFunction = nullptr;
    }

    void setRectangularGrid() {
        type = kLineByLine;
        xyFunction = nullptr;
        mLookUpTable.reset();
    }

    uint16_t mapToIndex(uint16_t x, uint16_t y) const {
        switch (type) {
        case kSeperentine:
            x = x % width;
            y = y % height;
            return xy_serpentine(x, y, width, height);
        case kLineByLine:
            return xy_line_by_line(x, y, width, height);
        case kFunction:
            x = x % width;
            y = y % height;
            return xyFunction(x, y, width, height);
        case kLookUpTable:
            return mLookUpTable->getData()[y * width + x];
        }
        return 0;
    }

    uint16_t getWidth() const { return width; }
    uint16_t getHeight() const { return height; }
    uint16_t getTotal() const { return width * height; }
    XyMapType getType() const { return type; }

  private:
    XYMap(uint16_t width, uint16_t height, XyMapType type)
        : type(type), width(width), height(height) {}

    XyMapType type;
    uint16_t width;
    uint16_t height;
    XYFunction xyFunction = nullptr;
    LUT16Ptr mLookUpTable; // optional refptr to look up table.
    uint16_t *mData = nullptr;  // direct pointer to look up table data.
};
