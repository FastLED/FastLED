


#include "fl/xmap.h"

namespace fl {

 

XMap XMap::constructWithUserFunction(uint16_t length, XFunction xFunction, uint16_t offset) {
    XMap out = XMap(length, kFunction);
    out.xFunction = xFunction;
    out.mOffset = offset;
    return out;
}



XMap XMap::constructWithLookUpTable(uint16_t length, const uint16_t *lookUpTable, uint16_t offset) {
    XMap out = XMap(length, kLookUpTable);
    out.mData = lookUpTable;
    out.mOffset = offset;
    return out;
}



XMap::XMap(uint16_t length, bool is_reverse, uint16_t offset) {
    type = is_reverse ? kReverse : kLinear;
    this->length = length;
    this->mOffset = offset;
}



XMap::XMap(const XMap &other) {
    type = other.type;
    length = other.length;
    xFunction = other.xFunction;
    mData = other.mData;
    mLookUpTable = other.mLookUpTable;
    mOffset = other.mOffset;
}



void XMap::convertToLookUpTable() {
    if (type == kLookUpTable) {
        return;
    }
    mLookUpTable.reset();
    mLookUpTable = LUT16Ptr::New(length);
    uint16_t* dataMutable = mLookUpTable->getData();
    mData = mLookUpTable->getData();
    for (uint16_t x = 0; x < length; x++) {
        dataMutable[x] = mapToIndex(x);
    }
    type = kLookUpTable;
    xFunction = nullptr;
}



uint16_t XMap::mapToIndex(uint16_t x) const {
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



uint16_t XMap::getLength() const {
    return length;
}



XMap::Type XMap::getType() const {
    return type;
}



XMap::XMap(uint16_t length, Type type)
    : length(length), type(type), mOffset(0) {
}



XMap &XMap::operator=(const XMap &other) {
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

} // namespace fl
