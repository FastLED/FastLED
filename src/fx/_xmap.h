#pragma once

#include <stdint.h>
#include <memory.h>
#include "ptr.h"

uint16_t x_serpentine(uint16_t x, uint16_t width) {
    if (x & 1) // Even or odd position?
        // reverse every second position for a serpentine layout
        return width - 1 - x;
    else
        return x;
}

uint16_t x_linear(uint16_t x, uint16_t width) {
    return x;
}

// typedef for xMap function type
typedef uint16_t (*XFunction)(uint16_t x, uint16_t width);

enum XMapType {
    kSerpentine = 0,
    kLinear,
    kFunction,
    kLookUpTable
};

// XMap holds either a function or a look up table to map x coordinates to a 1D index.
class XMap {
public:
    static XMap constructWithUserFunction(uint16_t width, XFunction xFunction) {
        XMap out = XMap(width, kFunction);
        out.xFunction = xFunction;
        return out;
    }

    static XMap constructWithLookUpTable(uint16_t width, const uint16_t *lookUpTable) {
        XMap out = XMap(width, kLookUpTable);
        out.lookUpTable = lookUpTable;
        return out;
    }

    // is_serpentine is true. You probably want this unless you are using a linear layout
    XMap(uint16_t width, bool is_serpentine = true) {
        type = is_serpentine ? kSerpentine : kLinear;
        this->width = width;
    }

    XMap(const XMap &other) {
        type = other.type;
        width = other.width;
        xFunction = other.xFunction;
        lookUpTable = other.lookUpTable;
        if (other.lookUpTableOwned.get()) {
            lookUpTableOwned.reset(new uint16_t[width]);
            memcpy(lookUpTableOwned.get(), lookUpTable, width * sizeof(uint16_t));
            lookUpTable = lookUpTableOwned.get();
        }
    }

    void optimizeAsLookupTable() {
        if (type == kLookUpTable) {
            return;
        }
        if (!lookUpTableOwned.get()) {
            lookUpTableOwned.reset(new uint16_t[width]);
        }
        lookUpTable = lookUpTableOwned.get();
        uint16_t* data = lookUpTableOwned.get();
        for (uint16_t x = 0; x < width; x++) {
            data[x] = mapToIndex(x);
        }
        type = kLookUpTable;
        xFunction = nullptr;
    }

    uint16_t mapToIndex(uint16_t x) const {
        switch (type) {
            case kSerpentine:
                x = x % width;
                return x_serpentine(x, width);
            case kLinear:
                return x_linear(x, width);
            case kFunction:
                x = x % width;
                return xFunction(x, width);
            case kLookUpTable:
                return lookUpTable[x];
        }
        return 0;
    }

    uint16_t getWidth() const {
        return width;
    }

    XMapType getType() const {
        return type;
    }

    void convertToLookUpTable() {
        if (type == kLookUpTable) {
            return;
        }
        if (!lookUpTableOwned.get()) {
            lookUpTableOwned.reset(new uint16_t[width]);
        }
        uint16_t *newLookUpTable = lookUpTableOwned.get();
        for (uint16_t x = 0; x < width; x++) {
            newLookUpTable[x] = mapToIndex(x);
        }
        type = kLookUpTable;
        xFunction = nullptr;
        lookUpTable = newLookUpTable;
    }

private:
    XMap(uint16_t width, XMapType type)
        : width(width), type(type) {
    }

    XMapType type = kSerpentine;
    uint16_t width = 0;
    XFunction xFunction = nullptr;
    const uint16_t *lookUpTable = nullptr;
    scoped_ptr<uint16_t> lookUpTableOwned;
};
