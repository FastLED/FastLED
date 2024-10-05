#pragma once

#include <stdint.h>
#include "ptr.h"
#include "force_inline.h"

// LUT16 holds a look up table to map data from one
// value to another. This can be quite big (1/3rd of the frame buffer)
// so a Referent is used to allow memory sharing.

class LUT16;
typedef RefPtr<LUT16> LUT16Ptr;
class LUT16 : public Referent {
public:
    LUT16(uint16_t length) : length(length) {
        data.reset(new uint16_t[length]);
    }

    FASTLED_FORCE_INLINE uint16_t* getData() const {
        return data.get();
    }
private:
    scoped_ptr<uint16_t> data;
    uint16_t length;
};
