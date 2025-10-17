#pragma once

#include "fl/stdint.h"

namespace fl {
    // Function declarations for memory management
    void* malloc(size_t size);
    void free(void* ptr);
    void* realloc(void* ptr, size_t new_size);

    // Function declaration for abs
    int abs(int x);
} // namespace fl

