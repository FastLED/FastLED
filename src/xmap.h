#pragma once

#include <stdint.h>
#include <string.h>
#include "ptr.h"
#include "force_inline.h"

FASTLED_FORCE_INLINE uint16_t x_linear(uint16_t x, uint16_t length) {
    return x;
}

FASTLED_FORCE_INLINE uint16_t x_reverse(uint16_t x, uint16_t length) {
    return length - 1 - x;
}

// typedef for xMap function type
typedef uint16_t (*XFunction)(uint16_t x, uint16_t length);



// XMap holds either a function or a look up table to map x coordinates to a 1D index.
class XMap {
public:

    enum Type {
        kLinear = 0,
        kReverse,
        kFunction,
        kLookUpTable
    };

    static XMap constructWithUserFunction(uint16_t length, XFunction xFunction) {
        XMap out = XMap(length, kFunction);
        out.xFunction = xFunction;
        return out;
    }

    static XMap constructWithLookUpTable(uint16_t length, const uint16_t *lookUpTable) {
        XMap out = XMap(length, kLookUpTable);
        out.lookUpTable = lookUpTable;
        return out;
    }

    // is_reverse is false by default for linear layout
    XMap(uint16_t length, bool is_reverse = false) {
        type = is_reverse ? kReverse : kLinear;
        this->length = length;
    }

    XMap(const XMap &other) {
        type = other.type;
        length = other.length;
        xFunction = other.xFunction;
        lookUpTable = other.lookUpTable;
        if (other.lookUpTableOwned.get()) {
            lookUpTableOwned.reset(new uint16_t[length]);
            memcpy(lookUpTableOwned.get(), lookUpTable, length * sizeof(uint16_t));
            lookUpTable = lookUpTableOwned.get();
        }
    }

    // define the assignment operator
    XMap& operator=(const XMap &other) {
        if (this != &other) {
            type = other.type;
            length = other.length;
            xFunction = other.xFunction;
            lookUpTable = other.lookUpTable;
            if (other.lookUpTableOwned.get()) {
                lookUpTableOwned.reset(new uint16_t[length]);
                memcpy(lookUpTableOwned.get(), lookUpTable, length * sizeof(uint16_t));
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
            lookUpTableOwned.reset(new uint16_t[length]);
        }
        lookUpTable = lookUpTableOwned.get();
        uint16_t* data = lookUpTableOwned.get();
        for (uint16_t x = 0; x < length; x++) {
            data[x] = mapToIndex(x);
        }
        type = kLookUpTable;
        xFunction = nullptr;
    }

    uint16_t mapToIndex(uint16_t x) const {
        switch (type) {
            case kLinear:
                return x_linear(x, length);
            case kReverse:
                return x_reverse(x, length);
            case kFunction:
                x = x % length;
                return xFunction(x, length);
            case kLookUpTable:
                return lookUpTable[x];
        }
        return 0;
    }

    uint16_t getLength() const {
        return length;
    }

    Type getType() const {
        return type;
    }

    void convertToLookUpTable() {
        if (type == kLookUpTable) {
            return;
        }
        if (!lookUpTableOwned.get()) {
            lookUpTableOwned.reset(new uint16_t[length]);
        }
        uint16_t *newLookUpTable = lookUpTableOwned.get();
        for (uint16_t x = 0; x < length; x++) {
            newLookUpTable[x] = mapToIndex(x);
        }
        type = kLookUpTable;
        xFunction = nullptr;
        lookUpTable = newLookUpTable;
    }


private:
    XMap(uint16_t length, Type type)
        : length(length), type(type) {
    }

    Type type = kLinear;
    uint16_t length = 0;
    XFunction xFunction = nullptr;
    const uint16_t *lookUpTable = nullptr;
    scoped_ptr<uint16_t> lookUpTableOwned;
};
