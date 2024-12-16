
#include <stdint.h>
#include <string.h>

#include "fl/force_inline.h"
#include "fl/namespace.h"
#include "fl/xymap.h"
#include "fl/screenmap.h"

using namespace fl;

namespace fl {


ScreenMap XYMap::toScreenMap() const {
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



XYMap XYMap::constructWithUserFunction(uint16_t width, uint16_t height,
                                       XYFunction xyFunction, uint16_t offset) {
    XYMap out(width, height, kFunction);
    out.xyFunction = xyFunction;
    out.mOffset = offset;
    return out;
}



XYMap XYMap::constructRectangularGrid(uint16_t width, uint16_t height, uint16_t offset) {
    XYMap out(width, height, kLineByLine);
    out.mOffset = offset;
    return out;
}



XYMap XYMap::constructWithLookUpTable(uint16_t width, uint16_t height,
                                      const uint16_t *lookUpTable, uint16_t offset) {
    XYMap out(width, height, kLookUpTable);
    out.mLookUpTable = LUT16Ptr::New(width * height);
    memcpy(out.mLookUpTable->getData(), lookUpTable,
           width * height * sizeof(uint16_t));
    out.mOffset = offset;
    return out;
}



XYMap::XYMap(uint16_t width, uint16_t height, bool is_serpentine, uint16_t offset)
    : type(is_serpentine ? kSerpentine : kLineByLine),
      width(width), height(height), mOffset(offset) {}



void XYMap::mapPixels(const CRGB* input, CRGB* output) const {
    uint16_t pos = 0;
    for (uint16_t y = 0; y < height; y++) {
        for (uint16_t x = 0; x < width; x++) {
            uint16_t i = pos++;
            output[i] = input[mapToIndex(x, y)];
        }
    }
}



void XYMap::convertToLookUpTable() {
    if (type == kLookUpTable) {
        return;
    }
    mLookUpTable = LUT16Ptr::New(width * height);
    uint16_t *data = mLookUpTable->getData();
    for (uint16_t y = 0; y < height; y++) {
        for (uint16_t x = 0; x < width; x++) {
            data[y * width + x] = mapToIndex(x, y);
        }
    }
    type = kLookUpTable;
    xyFunction = nullptr;
}



void XYMap::setRectangularGrid() {
    type = kLineByLine;
    xyFunction = nullptr;
    mLookUpTable.reset();
}



uint16_t XYMap::mapToIndex(uint16_t x, uint16_t y) const {
    uint16_t index;
    switch (type) {
    case kSerpentine:
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



uint16_t XYMap::getWidth() const { return width; }



uint16_t XYMap::getHeight() const { return height; }



uint16_t XYMap::getTotal() const { return width * height; }



XYMap::XyMapType XYMap::getType() const { return type; }



XYMap::XYMap(uint16_t width, uint16_t height, XyMapType type)
    : type(type), width(width), height(height), mOffset(0) {}

} // namespace fl

