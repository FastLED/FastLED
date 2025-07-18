

#include "fl/xmap.h"

namespace fl {

XMap XMap::constructWithUserFunction(u16 length, XFunction xFunction,
                                     u16 offset) {
    XMap out = XMap(length, kFunction);
    out.xFunction = xFunction;
    out.mOffset = offset;
    return out;
}

XMap XMap::constructWithLookUpTable(u16 length,
                                    const u16 *lookUpTable,
                                    u16 offset) {
    XMap out = XMap(length, kLookUpTable);
    out.mData = lookUpTable;
    out.mOffset = offset;
    return out;
}

XMap::XMap(u16 length, bool is_reverse, u16 offset) {
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
    mLookUpTable = fl::make_shared<LUT16>(length);
    u16 *dataMutable = mLookUpTable->getDataMutable();
    mData = mLookUpTable->getData();
    for (u16 x = 0; x < length; x++) {
        dataMutable[x] = mapToIndex(x);
    }
    type = kLookUpTable;
    xFunction = nullptr;
}

u16 XMap::mapToIndex(u16 x) const {
    u16 index;
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

u16 XMap::getLength() const { return length; }

XMap::Type XMap::getType() const { return type; }

XMap::XMap(u16 length, Type type)
    : length(length), type(type), mOffset(0) {}

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
