#pragma once

#include <stdint.h>
#include <string.h>

#include "fl/force_inline.h"
#include "fl/lut.h"
#include "fl/ptr.h"

#include "fl/namespace.h"

namespace fl {

FASTLED_FORCE_INLINE uint16_t x_linear(uint16_t x, uint16_t length) {
    (void)length;
    return x;
}

FASTLED_FORCE_INLINE uint16_t x_reverse(uint16_t x, uint16_t length) {
    return length - 1 - x;
}

// typedef for xMap function type
typedef uint16_t (*XFunction)(uint16_t x, uint16_t length);

// XMap holds either a function or a look up table to map x coordinates to a 1D
// index.
class XMap {
  public:
    enum Type { kLinear = 0, kReverse, kFunction, kLookUpTable };

    static XMap constructWithUserFunction(uint16_t length, XFunction xFunction,
                                          uint16_t offset = 0);

    // When a pointer to a lookup table is passed in then we assume it's
    // owned by someone else and will not be deleted.
    static XMap constructWithLookUpTable(uint16_t length,
                                         const uint16_t *lookUpTable,
                                         uint16_t offset = 0);

    // is_reverse is false by default for linear layout
    XMap(uint16_t length, bool is_reverse = false, uint16_t offset = 0);

    XMap(const XMap &other);

    // define the assignment operator
    XMap &operator=(const XMap &other) ;

    void convertToLookUpTable();

    uint16_t mapToIndex(uint16_t x) const;

    uint16_t operator()(uint16_t x) const { return mapToIndex(x); }

    uint16_t getLength() const;

    Type getType() const;

  private:
    XMap(uint16_t length, Type type);
    uint16_t length = 0;
    Type type = kLinear;
    XFunction xFunction = nullptr;
    const uint16_t *mData = nullptr;
    fl::LUT16Ptr mLookUpTable;
    uint16_t mOffset = 0; // offset to be added to the output
};

} // namespace fl
