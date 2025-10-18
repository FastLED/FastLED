#pragma once

#include "fl/int.h"

// Forward declarations instead of including <string.h>
// to avoid heavy C standard library header overhead
extern "C" {
    void* memset(void* ptr, int value, fl::size num);
    void* memcpy(void* dst, const void* src, fl::size num);
    void* memmove(void* dst, const void* src, fl::size num);
    char* strstr(const char* haystack, const char* needle);
    int strncmp(const char* s1, const char* s2, fl::size n);
}

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


// fl::memmove - wrapper for memmove (C library function)
void* memmove(void* dst, const void* src, fl::size num);

// fl::strstr - wrapper for strstr (C library function)
// Note: Returns char* for C compatibility, cast to const if needed
const char* strstr(const char* haystack, const char* needle);

// fl::strncmp - wrapper for strncmp (C library function)
int strncmp(const char* s1, const char* s2, fl::size n);



} // namespace fl
