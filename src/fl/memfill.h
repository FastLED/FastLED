#pragma once

#include <string.h>  // for standard memset fallback

#include "fl/int.h"

namespace fl {


// Overload for void* to maintain standard memset signature
inline void* memfill(void* ptr, int value, fl::size num) {
    return ::memset(ptr, value, num);
}


// fl::memfill - provides a FastLED-specific memfill implementation
// that is compatible across all supported platforms
template <typename T>
inline void* memfill(T* ptr, int value, fl::size num) {
    union memfill_union {  // For type aliasing safety.
        T* ptr;
        void* void_ptr;
    };
    memfill_union u;
    u.ptr = ptr;
    return fl::memfill(u.void_ptr, value, num);
}


inline void* memcopy(void* dst, const void* src, fl::size num) {
    return ::memcpy(dst, src, num);
}



} // namespace fl
