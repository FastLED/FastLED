#pragma once

#include "fl/stl/stdint.h"
#include "fl/exit.h"

namespace fl {
    // Function declarations for memory management
    void* malloc(size_t size);
    void free(void* ptr);
    void* calloc(size_t nmemb, size_t size);
    void* realloc(void* ptr, size_t new_size);
} // namespace fl

