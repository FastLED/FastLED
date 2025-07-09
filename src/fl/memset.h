#pragma once

#include <string.h>  // for standard memset fallback

#include "fl/int.h"

namespace fl {


// Overload for void* to maintain standard memset signature
inline void* memset(void* ptr, int value, fl::size num) {
    return ::memset(ptr, value, num);
}


// fl::memset - provides a FastLED-specific memset implementation
// that is compatible across all supported platforms
template <typename T>
inline void* memset(T* ptr, int value, fl::size num) {
    union memset_union {  // For type aliasing safety.
        T* ptr;
        void* void_ptr;
    };
    memset_union u;
    u.ptr = ptr;
    return fl::memset(u.void_ptr, value, num);
}


inline void* memcopy(void* dst, const void* src, fl::size num) {
    return ::memcpy(dst, src, num);
}



} // namespace fl
