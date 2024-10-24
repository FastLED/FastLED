#pragma once

/*
LUT - Look up table implementation for various types.
*/

#include <stdint.h>
#include "ptr.h"
#include "force_inline.h"
#include "allocator.h"

template<typename T>
struct pair_xy {
    T x = 0;
    T y = 0;
    constexpr pair_xy() = default;
    constexpr pair_xy(T x, T y) : x(x), y(y) {}
};

using pair_xy_float = pair_xy<float>;  // It's just easier if we allow negative values.

// LUT holds a look up table to map data from one
// value to another. This can be quite big (1/3rd of the frame buffer)
// so a Referent is used to allow memory sharing.

template<typename T>
class LUT;

typedef LUT<uint16_t> LUT16;
typedef LUT<pair_xy_float> LUTXYFLOAT;

DECLARE_SMART_PTR_NO_FWD(LUT16);
DECLARE_SMART_PTR_NO_FWD(LUTXYFLOAT);

// Templated lookup table.
template<typename T>
class LUT : public Referent {
public:
    friend class PtrTraits<LUT<T>>;
    LUT(uint32_t length) : length(length) {
        T* ptr = LargeBlockAllocator<T>::Alloc(length);
        mDataHandle.reset(ptr);
        data = ptr;
    }
    // In this version the data is passed in but not managed by this object.
    LUT(uint32_t length, T* data) : length(length) {
        this->data = data;
    }
    ~LUT() {
        LargeBlockAllocator<T>::Free(mDataHandle.release(), length);
        data = mDataHandle.get();
    }

    const T& operator[](uint32_t index) const {
        return data[index];
    }

    const T& operator[](uint16_t index) const {
        return data[index];
    }

    T* getData() const {
        return data;
    }
private:
    scoped_ptr<T> mDataHandle;
    T* data = nullptr;
    uint32_t length;
};

