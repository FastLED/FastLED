#pragma once

#include "fl/stl/compiler_control.h"
#include "fl/math/lut.h"
#include "fl/stl/int.h"
#include "fl/stl/noexcept.h"

namespace fl {

FASTLED_FORCE_INLINE u16 x_linear(u16 x, u16 length) FL_NO_EXCEPT {
    (void)length;
    return x;
}

FASTLED_FORCE_INLINE u16 x_reverse(u16 x, u16 length) FL_NO_EXCEPT {
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
                                          u16 offset = 0) FL_NO_EXCEPT;

    // When a pointer to a lookup table is passed in then we assume it's
    // owned by someone else and will not be deleted.
    static XMap constructWithLookUpTable(u16 length,
                                         const u16 *lookUpTable,
                                         u16 offset = 0) FL_NO_EXCEPT;

    // is_reverse is false by default for linear layout
    XMap(u16 length, bool is_reverse = false, u16 offset = 0) FL_NO_EXCEPT;

    XMap(const XMap &other) FL_NO_EXCEPT;

    // define the assignment operator
    XMap &operator=(const XMap &other) FL_NO_EXCEPT;

    void convertToLookUpTable() FL_NO_EXCEPT;

    u16 mapToIndex(u16 x) const FL_NO_EXCEPT;

    u16 operator()(u16 x) const FL_NO_EXCEPT { return mapToIndex(x); }

    u16 getLength() const FL_NO_EXCEPT;

    Type getType() const FL_NO_EXCEPT;

  private:
    XMap(u16 length, Type type) FL_NO_EXCEPT;
    u16 length = 0;
    Type type = kLinear;
    XFunction xFunction = nullptr;
    const u16 *mData = nullptr;
    fl::LUT16Ptr mLookUpTable;
    u16 mOffset = 0; // offset to be added to the output
};

} // namespace fl
