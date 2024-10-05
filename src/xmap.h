#pragma once

#include <stdint.h>
#include <string.h>
#include "ptr.h"
#include "force_inline.h"
#include "lut16.h"

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

    // When a pointer to a lookup table is passed in then we assume it's
    // owned by someone else and will not be deleted.
    static XMap constructWithLookUpTable(uint16_t length, const uint16_t *lookUpTable) {
        XMap out = XMap(length, kLookUpTable);
        out.mData = lookUpTable;
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
        mData = other.mData;
        mLookUpTable = other.mLookUpTable;
    }

    // define the assignment operator
    XMap& operator=(const XMap &other) {
        if (this != &other) {
            type = other.type;
            length = other.length;
            xFunction = other.xFunction;
            mData = other.mData;
            mLookUpTable = other.mLookUpTable;
        }
        return *this;
    }

    void convertToLookUpTable() {
        if (type == kLookUpTable) {
            return;
        }
        mLookUpTable.reset();
        mLookUpTable = LUT16Ptr::FromHeap(new LUT16(length));
        uint16_t* dataMutable = mLookUpTable->getData();
        mData = mLookUpTable->getData();
        for (uint16_t x = 0; x < length; x++) {
            dataMutable[x] = mapToIndex(x);
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
                return mData[x];
        }
        return 0;
    }

    uint16_t getLength() const {
        return length;
    }

    Type getType() const {
        return type;
    }


private:
    XMap(uint16_t length, Type type)
        : length(length), type(type) {
    }
    uint16_t length = 0;
    Type type = kLinear;
    XFunction xFunction = nullptr;
    const uint16_t *mData = nullptr;
    LUT16Ptr mLookUpTable;
};
