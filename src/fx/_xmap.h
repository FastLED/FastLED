#pragma once

#include <stdint.h>
#include <string.h>
#include "ptr.h"
#include "force_inline.h"

FASTLED_FORCE_INLINE uint16_t x_linear(uint16_t x, uint16_t width) {
    return x;
}

FASTLED_FORCE_INLINE uint16_t x_reverse(uint16_t x, uint16_t width) {
    return width - 1 - x;
}

// typedef for xMap function type
typedef uint16_t (*XFunction)(uint16_t x, uint16_t width);



// XMap holds either a function or a look up table to map x coordinates to a 1D index.
class XMap {
public:

    enum Type {
        kLinear = 0,
        kReverse,
        kFunction,
        kLookUpTable
    };

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

    // is_reverse is false by default for linear layout
    XMap(uint16_t width, bool is_reverse = false) {
        type = is_reverse ? kReverse : kLinear;
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

    // define the assignment operator
    XMap& operator=(const XMap &other) {
        if (this != &other) {
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
        return *this;
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
            case kLinear:
                return x_linear(x, width);
            case kReverse:
                return x_reverse(x, width);
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

    Type getType() const {
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
    XMap(uint16_t width, Type type)
        : width(width), type(type) {
    }

    Type type = kLinear;
    uint16_t width = 0;
    XFunction xFunction = nullptr;
    const uint16_t *lookUpTable = nullptr;
    scoped_ptr<uint16_t> lookUpTableOwned;
};
