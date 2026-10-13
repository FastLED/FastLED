#pragma once

#include "fl/stl/stdint.h"
#include "fl/stl/noexcept.h"

namespace fl {
    // Function declarations for memory management
    void* malloc(size_t size) FL_NO_EXCEPT;
    void free(void* ptr) FL_NO_EXCEPT;
    void* calloc(size_t nmemb, size_t size) FL_NO_EXCEPT;
    void* realloc(void* ptr, size_t new_size) FL_NO_EXCEPT;

    // Typed-pointer overloads: keep unqualified `free(leds)` / `realloc(leds, n)`
    // on fl::CRGB* unambiguous against ::free / ::realloc (see fl/stl/cstring.h).
    template <typename T>
    inline void free(T* ptr) FL_NO_EXCEPT {
        free(static_cast<void*>(ptr));
    }
    template <typename T>
    inline void* realloc(T* ptr, size_t new_size) FL_NO_EXCEPT {
        return realloc(static_cast<void*>(ptr), new_size);
    }
} // namespace fl
