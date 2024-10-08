#pragma once

#include <stdint.h>
#include "ptr.h"
#include "force_inline.h"
#include "allocator.h"

// LUT16 holds a look up table to map data from one
// value to another. This can be quite big (1/3rd of the frame buffer)
// so a Referent is used to allow memory sharing.

class LUT16;
typedef Ptr<LUT16> LUT16Ptr;
class LUT16 : public Referent {
public:
    friend class PtrTraits<LUT16>;
    LUT16(uint16_t length) : length(length) {
        uint16_t* ptr = LargeBlockAllocator<uint16_t>::Alloc(length);
        mDataHandle.reset(ptr);
        data = ptr;
    }
    // In this version the data is passed in but not managed by this object.
    LUT16(uint16_t length, uint16_t* data) : length(length) {
        this->data = data;
    }
    ~LUT16() {
        LargeBlockAllocator<uint16_t>::Free(mDataHandle.release(), length);
        data = mDataHandle.get();
    }

    FASTLED_FORCE_INLINE uint16_t* getData() const {
        return data;
    }
private:
    scoped_ptr<uint16_t> mDataHandle;
    uint16_t* data = nullptr;
    uint16_t length;
};
