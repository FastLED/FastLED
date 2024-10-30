#pragma once

#include <stdint.h>
#include <string.h>

#include "force_inline.h"
#include "ref.h"
#include "xmap.h" // Include xmap.h for LUT16
#include "lut.h"
#include "crgb.h"
#include "screenmap.h"
#include "namespace.h"

FASTLED_NAMESPACE_BEGIN

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
                                           XYFunction xyFunction, uint16_t offset = 0) {
        XYMap out(width, height, kFunction);
        out.xyFunction = xyFunction;
        out.mOffset = offset;
        return out;
    }

    static XYMap constructRectangularGrid(uint16_t width, uint16_t height, uint16_t offset = 0) {
        XYMap out(width, height, kLineByLine);
        out.mOffset = offset;
        return out;
    }

    static XYMap constructWithLookUpTable(uint16_t width, uint16_t height,
                                          const uint16_t *lookUpTable, uint16_t offset = 0) {
        XYMap out(width, height, kLookUpTable);
        out.mLookUpTable = LUT16Ref::New(width * height);
        memcpy(out.mLookUpTable->getData(), lookUpTable,
               width * height * sizeof(uint16_t));
        out.mOffset = offset;
        return out;
    }

    // is_serpentine is true by default. You probably want this unless you are
    // using a different layout
    XYMap(uint16_t width, uint16_t height, bool is_serpentine = true, uint16_t offset = 0)
        : type(is_serpentine ? kSeperentine : kLineByLine),
          width(width), height(height), mOffset(offset) {}

    XYMap(const XYMap &other) = default;

    ScreenMap toScreenMap() const {
        const uint16_t length = width * height;
        ScreenMap out(length);
        for (uint16_t w = 0; w < width; w++) {
            for (uint16_t h = 0; h < height; h++) {
                uint16_t index = mapToIndex(w, h);
                pair_xy_float p = {
                    static_cast<float>(w),
                    static_cast<float>(h)
                };
                out.set(index, p);
            }
        }
        return out;
    }

    void mapPixels(const CRGB* input, CRGB* output) const {
        uint16_t pos = 0;
        for (uint16_t y = 0; y < height; y++) {
            for (uint16_t x = 0; x < width; x++) {
                uint16_t i = pos++;
                output[i] = input[mapToIndex(x, y)];
            }
        }
    }

    void convertToLookUpTable() {
        if (type == kLookUpTable) {
            return;
        }
        mLookUpTable = LUT16Ref::New(width * height);
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
        uint16_t index;
        switch (type) {
        case kSeperentine:
            x = x % width;
            y = y % height;
            index = xy_serpentine(x, y, width, height);
            break;
        case kLineByLine:
            index = xy_line_by_line(x, y, width, height);
            break;
        case kFunction:
            x = x % width;
            y = y % height;
            index = xyFunction(x, y, width, height);
            break;
        case kLookUpTable:
            index = mLookUpTable->getData()[y * width + x];
            break;
        default:
            return 0;
        }
        return index + mOffset;
    }

    uint16_t getWidth() const { return width; }
    uint16_t getHeight() const { return height; }
    uint16_t getTotal() const { return width * height; }
    XyMapType getType() const { return type; }

  private:
    XYMap(uint16_t width, uint16_t height, XyMapType type)
        : type(type), width(width), height(height), mOffset(0) {}

    XyMapType type;
    uint16_t width;
    uint16_t height;
    XYFunction xyFunction = nullptr;
    LUT16Ref mLookUpTable; // optional refptr to look up table.
    uint16_t mOffset = 0;  // offset to be added to the output
};

FASTLED_NAMESPACE_END