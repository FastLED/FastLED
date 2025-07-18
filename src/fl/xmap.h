#pragma once

#include "fl/stdint.h"
#include <string.h>

#include "fl/force_inline.h"
#include "fl/lut.h"
#include "fl/memory.h"

#include "fl/namespace.h"

namespace fl {

FASTLED_FORCE_INLINE u16 x_linear(u16 x, u16 length) {
    (void)length;
    return x;
}

FASTLED_FORCE_INLINE u16 x_reverse(u16 x, u16 length) {
    return length - 1 - x;
}

// typedef for xMap function type
typedef u16 (*XFunction)(u16 x, u16 length);

// XMap holds either a function or a look up table to map x coordinates to a 1D
// index.
class XMap {
  public:
    enum Type { kLinear = 0, kReverse, kFunction, kLookUpTable };

    static XMap constructWithUserFunction(u16 length, XFunction xFunction,
                                          u16 offset = 0);

    // When a pointer to a lookup table is passed in then we assume it's
    // owned by someone else and will not be deleted.
    static XMap constructWithLookUpTable(u16 length,
                                         const u16 *lookUpTable,
                                         u16 offset = 0);

    // is_reverse is false by default for linear layout
    XMap(u16 length, bool is_reverse = false, u16 offset = 0);

    XMap(const XMap &other);

    // define the assignment operator
    XMap &operator=(const XMap &other);

    void convertToLookUpTable();

    u16 mapToIndex(u16 x) const;

    u16 operator()(u16 x) const { return mapToIndex(x); }

    u16 getLength() const;

    Type getType() const;

  private:
    XMap(u16 length, Type type);
    u16 length = 0;
    Type type = kLinear;
    XFunction xFunction = nullptr;
    const u16 *mData = nullptr;
    fl::LUT16Ptr mLookUpTable;
    u16 mOffset = 0; // offset to be added to the output
};

} // namespace fl
