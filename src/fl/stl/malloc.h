#pragma once

#include "fl/stl/stdint.h"
#include "fl/stl/compiler_control.h"
#include "fl/stl/noexcept.h"

namespace fl {
    // Implementations. The public fl::name functions below are templates with an
    // unused defaulted parameter, so wherever the C library version is also
    // visible (ADL on an fl type, or a using-directive for fl) the non-template
    // ::name wins the tie instead of the call being ambiguous (FastLED#4827).
    namespace libc_impl {
    void* malloc(size_t size) FL_NO_EXCEPT;
    void free(void* ptr) FL_NO_EXCEPT;
    void* calloc(size_t nmemb, size_t size) FL_NO_EXCEPT;
    void* realloc(void* ptr, size_t new_size) FL_NO_EXCEPT;
    }  // namespace libc_impl

    // Function declarations for memory management
    template <typename FlLibcTag = void>
    FASTLED_FORCE_INLINE void* malloc(size_t size) FL_NO_EXCEPT { return libc_impl::malloc(size); }
    template <typename FlLibcTag = void>
    FASTLED_FORCE_INLINE void free(void* ptr) FL_NO_EXCEPT { return libc_impl::free(ptr); }
    template <typename FlLibcTag = void>
    FASTLED_FORCE_INLINE void* calloc(size_t nmemb, size_t size) FL_NO_EXCEPT { return libc_impl::calloc(nmemb, size); }
    template <typename FlLibcTag = void>
    FASTLED_FORCE_INLINE void* realloc(void* ptr, size_t new_size) FL_NO_EXCEPT { return libc_impl::realloc(ptr, new_size); }
} // namespace fl
