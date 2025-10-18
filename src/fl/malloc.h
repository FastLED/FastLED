#pragma once

#include "fl/stdint.h"

namespace fl {
    // Function declarations for memory management
    void* malloc(size_t size);
    void free(void* ptr);
    void* calloc(size_t nmemb, size_t size);
    void* realloc(void* ptr, size_t new_size);

    // Function declarations for control flow
    void exit(int status);
} // namespace fl

