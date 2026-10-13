#pragma once

#include "fl/stl/stdint.h"
#include "fl/stl/noexcept.h"

namespace fl {
    // Function declarations for memory management
    void* malloc(size_t size) FL_NO_EXCEPT;
    void free(void* ptr) FL_NO_EXCEPT;
    void* calloc(size_t nmemb, size_t size) FL_NO_EXCEPT;
    void* realloc(void* ptr, size_t new_size) FL_NO_EXCEPT;
} // namespace fl
