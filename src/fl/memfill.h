#pragma once

#include "fl/int.h"

namespace fl {

// fl::memfill - provides a FastLED-specific memfill implementation
// that is compatible across all supported platforms

// Overload for void* to maintain standard memset signature
void* memfill(void* ptr, int value, fl::size num) noexcept;

// Templated version for type-safe operations
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

// fl::memcopy - wrapper for memcpy
void* memcopy(void* dst, const void* src, fl::size num) noexcept;

// fl::memmove - wrapper for memmove
void* memmove(void* dst, const void* src, fl::size num) noexcept;

// fl::strstr - wrapper for strstr
// Note: Returns char* for C compatibility, cast to const if needed
const char* strstr(const char* haystack, const char* needle) noexcept;

// fl::strncmp - wrapper for strncmp
int strncmp(const char* s1, const char* s2, fl::size n) noexcept;

} // namespace fl
