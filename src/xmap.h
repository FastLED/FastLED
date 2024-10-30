#pragma once

#include <stdint.h>
#include <string.h>

#include "ref.h"
#include "force_inline.h"
#include "lut.h"

#include "namespace.h"

FASTLED_NAMESPACE_BEGIN

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

    static XMap constructWithUserFunction(uint16_t length, XFunction xFunction, uint16_t offset = 0) {
        XMap out = XMap(length, kFunction);
        out.xFunction = xFunction;
        out.mOffset = offset;
        return out;
    }

    // When a pointer to a lookup table is passed in then we assume it's
    // owned by someone else and will not be deleted.
    static XMap constructWithLookUpTable(uint16_t length, const uint16_t *lookUpTable, uint16_t offset = 0) {
        XMap out = XMap(length, kLookUpTable);
        out.mData = lookUpTable;
        out.mOffset = offset;
        return out;
    }

    // is_reverse is false by default for linear layout
    XMap(uint16_t length, bool is_reverse = false, uint16_t offset = 0) {
        type = is_reverse ? kReverse : kLinear;
        this->length = length;
        this->mOffset = offset;
    }

    XMap(const XMap &other) {
        type = other.type;
        length = other.length;
        xFunction = other.xFunction;
        mData = other.mData;
        mLookUpTable = other.mLookUpTable;
        mOffset = other.mOffset;
    }

    // define the assignment operator
    XMap& operator=(const XMap &other) {
        if (this != &other) {
            type = other.type;
            length = other.length;
            xFunction = other.xFunction;
            mData = other.mData;
            mLookUpTable = other.mLookUpTable;
            mOffset = other.mOffset;
        }
        return *this;
    }

    void convertToLookUpTable() {
        if (type == kLookUpTable) {
            return;
        }
        mLookUpTable.reset();
        mLookUpTable = LUT16Ref::New(length);
        uint16_t* dataMutable = mLookUpTable->getData();
        mData = mLookUpTable->getData();
        for (uint16_t x = 0; x < length; x++) {
            dataMutable[x] = mapToIndex(x);
        }
        type = kLookUpTable;
        xFunction = nullptr;
    }

    uint16_t mapToIndex(uint16_t x) const {
        uint16_t index;
        switch (type) {
            case kLinear:
                index = x_linear(x, length);
                break;
            case kReverse:
                index = x_reverse(x, length);
                break;
            case kFunction:
                x = x % length;
                index = xFunction(x, length);
                break;
            case kLookUpTable:
                index = mData[x];
                break;
            default:
                return 0;
        }
        return index + mOffset;
    }

    uint16_t getLength() const {
        return length;
    }

    Type getType() const {
        return type;
    }


private:
    XMap(uint16_t length, Type type)
        : length(length), type(type), mOffset(0) {
    }
    uint16_t length = 0;
    Type type = kLinear;
    XFunction xFunction = nullptr;
    const uint16_t *mData = nullptr;
    LUT16Ref mLookUpTable;
    uint16_t mOffset = 0;  // offset to be added to the output
};

FASTLED_NAMESPACE_END
