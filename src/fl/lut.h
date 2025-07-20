#pragma once

/*
LUT - Look up table implementation for various types.
*/

#include "fl/allocator.h"
#include "fl/force_inline.h"
#include "fl/memory.h"
#include "fl/stdint.h"

#include "fl/int.h"
#include "fl/geometry.h"
#include "fl/namespace.h"

namespace fl {

// LUT holds a look up table to map data from one
// value to another. This can be quite big (1/3rd of the frame buffer)
// so a Referent is used to allow memory sharing.

template <typename T> class LUT;

typedef LUT<u16> LUT16;
typedef LUT<vec2<u16>> LUTXY16;
typedef LUT<vec2f> LUTXYFLOAT;
typedef LUT<vec3f> LUTXYZFLOAT;

FASTLED_SMART_PTR_NO_FWD(LUT16);
FASTLED_SMART_PTR_NO_FWD(LUTXY16);
FASTLED_SMART_PTR_NO_FWD(LUTXYFLOAT);
FASTLED_SMART_PTR_NO_FWD(LUTXYZFLOAT);

// Templated lookup table.
template <typename T> class LUT {
  public:
    LUT(u32 length) : length(length) {
        T *ptr = PSRamAllocator<T>::Alloc(length);
        mDataHandle.reset(ptr);
        data = ptr;
    }
    // In this version the data is passed in but not managed by this object.
    LUT(u32 length, T *data) : length(length) { this->data = data; }
    ~LUT() {
        PSRamAllocator<T>::Free(mDataHandle.release());
        data = mDataHandle.get();
    }

    const T &operator[](u32 index) const { return data[index]; }

    const T &operator[](u16 index) const { return data[index]; }

    T *getDataMutable() { return data; }

    const T *getData() const { return data; }

    u32 size() const { return length; }

    T interp8(u8 alpha) {
        if (length == 0)
            return T();
        if (alpha == 0)
            return data[0];
        if (alpha == 255)
            return data[length - 1];

        // treat alpha/255 as fraction, scale to [0..length-1]
        u32 maxIndex = length - 1;
        u32 pos = u32(alpha) * maxIndex; // numerator
        u32 idx0 = pos / 255;                 // floor(position)
        u32 idx1 = idx0 < maxIndex ? idx0 + 1 : maxIndex;
        u8 blend = pos % 255; // fractional part

        const T &a = data[idx0];
        const T &b = data[idx1];
        // a + (b-a) * blend/255
        return a + (b - a) * blend / 255;
    }

    T interp16(u16 alpha) {
        if (length == 0)
            return T();
        if (alpha == 0)
            return data[0];
        if (alpha == 65535)
            return data[length - 1];

        // treat alpha/65535 as fraction, scale to [0..length-1]
        u32 maxIndex = length - 1;
        u32 pos = u32(alpha) * maxIndex; // numerator
        u32 idx0 = pos / 65535;               // floor(position)
        u32 idx1 = idx0 < maxIndex ? idx0 + 1 : maxIndex;
        u16 blend = pos % 65535; // fractional part

        const T &a = data[idx0];
        const T &b = data[idx1];
        // a + (b-a) * blend/65535
        return a + (b - a) * blend / 65535;
    }

  private:
    fl::unique_ptr<T> mDataHandle;
    T *data = nullptr;
    u32 length;
};

} // namespace fl
