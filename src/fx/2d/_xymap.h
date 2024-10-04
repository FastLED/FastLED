#pragma once

#include <stdint.h>
#include <string.h>
#include "ptr.h"
#include "force_inline.h"

FASTLED_FORCE_INLINE uint16_t xy_serpentine(uint16_t x, uint16_t y, uint16_t width, uint16_t height) {
    if (y & 1) // Even or odd row?
        // reverse every second line for a serpentine lled layout
        return (y + 1) * width - 1 - x;
    else
        return y * width + x;
}

FASTLED_FORCE_INLINE uint16_t xy_line_by_line(uint16_t x, uint16_t y, uint16_t width, uint16_t height) {
    return y * width + x;
}


// typedef for xyMap function type
typedef uint16_t (*XYFunction)(uint16_t x, uint16_t y, uint16_t width, uint16_t height);

enum XyMapType {
    kSeperentine = 0,
    kLineByLine,
    kFunction,
    kLookUpTable
};

// XYMap holds either a function or a look up table to map x, y coordinates to a 1D index.
class  XYMap {
public:
    static XYMap constructWithUserFunction(uint16_t width, uint16_t height, XYFunction xyFunction) {
        XYMap out = XYMap(width, height, kFunction);
        out.xyFunction = xyFunction;
        return out;
    }

    static XYMap constructWithLookUpTable(uint16_t width, uint16_t height, const uint16_t *lookUpTable) {
        XYMap out = XYMap(width, height, kLookUpTable);
        out.lookUpTable = lookUpTable;
        return out;
    }

    // is_superentine is true. You probably want this unless you are 
    XYMap(uint16_t width, uint16_t height, bool is_serpentine = true) {
        type = is_serpentine ? kSeperentine : kLineByLine;
        this->width = width;
        this->height = height;
    }

    XYMap(const XYMap &other) {
        type = other.type;
        width = other.width;
        height = other.height;
        xyFunction = other.xyFunction;
        lookUpTable = other.lookUpTable;
        if (other.lookUpTableOwned.get()) {
            lookUpTableOwned.reset(new uint16_t[width * height]);
            memcpy(lookUpTableOwned.get(), lookUpTable, width * height * sizeof(uint16_t));
            lookUpTable = lookUpTableOwned.get();
        }
    }

    void optimizeAsLookupTable() {
        if (type == kLookUpTable) {
            return;
        }
        if (!lookUpTableOwned.get()) {
            lookUpTableOwned.reset(new uint16_t[width * height]);
        }
        lookUpTable = lookUpTableOwned.get();
        uint16_t* data = lookUpTableOwned.get();
        for (uint16_t y = 0; y < height; y++) {
            for (uint16_t x = 0; x < width; x++) {
                data[y * width + x] = mapToIndex(x, y);
            }
        }
        type = kLookUpTable;
        xyFunction = nullptr;
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
                return lookUpTable[y * width + x];
        }
        return 0;
    }

    uint16_t getWidth() const {
        return width;
    }

    uint16_t getHeight() const {
        return height;
    }

    XyMapType getType() const {
        return type;
    }

    void convertToLookUpTable() {
        if (type == kLookUpTable) {
            return;
        }
        if (!lookUpTableOwned.get()) {
            lookUpTableOwned.reset(new uint16_t[width * height]);
        }
        uint16_t *newLookUpTable = lookUpTableOwned.get();
        for (uint16_t y = 0; y < height; y++) {
            for (uint16_t x = 0; x < width; x++) {
                newLookUpTable[y * width + x] = mapToIndex(x, y);
            }
        }
        type = kLookUpTable;
        xyFunction = nullptr;
        lookUpTable = newLookUpTable;
    }

private:
    XYMap(uint16_t width, uint16_t height, XyMapType type)
        : width(width), height(height), type(type) {
    }

    XyMapType type = kSeperentine;
    uint16_t width = 0;
    uint16_t height = 0;
    XYFunction xyFunction = nullptr;
    const uint16_t *lookUpTable = nullptr;
    scoped_ptr<uint16_t> lookUpTableOwned;

};
