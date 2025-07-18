
#include "fl/stdint.h"
#include <string.h>

#include "fl/clamp.h"
#include "fl/force_inline.h"
#include "fl/namespace.h"
#include "fl/screenmap.h"
#include "fl/xymap.h"

namespace fl {

ScreenMap XYMap::toScreenMap() const {
    const u16 length = width * height;
    ScreenMap out(length);
    for (u16 w = 0; w < width; w++) {
        for (u16 h = 0; h < height; h++) {
            u16 index = mapToIndex(w, h);
            vec2f p = {static_cast<float>(w), static_cast<float>(h)};
            out.set(index, p);
        }
    }
    return out;
}

XYMap XYMap::constructWithUserFunction(u16 width, u16 height,
                                       XYFunction xyFunction, u16 offset) {
    XYMap out(width, height, kFunction);
    out.xyFunction = xyFunction;
    out.mOffset = offset;
    return out;
}

XYMap XYMap::constructRectangularGrid(u16 width, u16 height,
                                      u16 offset) {
    XYMap out(width, height, kLineByLine);
    out.mOffset = offset;
    return out;
}

XYMap XYMap::constructWithLookUpTable(u16 width, u16 height,
                                      const u16 *lookUpTable,
                                      u16 offset) {
    XYMap out(width, height, kLookUpTable);
    out.mLookUpTable = fl::make_shared<LUT16>(width * height);
    memcpy(out.mLookUpTable->getDataMutable(), lookUpTable,
           width * height * sizeof(u16));
    out.mOffset = offset;
    return out;
}

XYMap XYMap::constructSerpentine(u16 width, u16 height,
                                 u16 offset) {
    XYMap out(width, height, true);
    out.mOffset = offset;
    return out;
}

XYMap::XYMap(u16 width, u16 height, bool is_serpentine,
             u16 offset)
    : type(is_serpentine ? kSerpentine : kLineByLine), width(width),
      height(height), mOffset(offset) {}

void XYMap::mapPixels(const CRGB *input, CRGB *output) const {
    u16 pos = 0;
    for (u16 y = 0; y < height; y++) {
        for (u16 x = 0; x < width; x++) {
            u16 i = pos++;
            output[i] = input[mapToIndex(x, y)];
        }
    }
}

void XYMap::convertToLookUpTable() {
    if (type == kLookUpTable) {
        return;
    }
    mLookUpTable = fl::make_shared<LUT16>(width * height);
    u16 *data = mLookUpTable->getDataMutable();
    for (u16 y = 0; y < height; y++) {
        for (u16 x = 0; x < width; x++) {
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

u16 XYMap::mapToIndex(const u16 &x, const u16 &y) const {
    u16 index;
    switch (type) {
    case kSerpentine: {
        u16 xx = x % width;
        u16 yy = y % height;
        index = xy_serpentine(xx, yy, width, height);
        break;
    }
    case kLineByLine: {
        u16 xx = x % width;
        u16 yy = y % height;
        index = xy_line_by_line(xx, yy, width, height);
        break;
    }
    case kFunction:
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

u16 XYMap::getWidth() const { return width; }

u16 XYMap::getHeight() const { return height; }

u16 XYMap::getTotal() const { return width * height; }

XYMap::XyMapType XYMap::getType() const { return type; }

XYMap::XYMap(u16 width, u16 height, XyMapType type)
    : type(type), width(width), height(height), mOffset(0) {}

} // namespace fl
