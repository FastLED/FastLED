#include "fl/stl/malloc.h"
#include "fl/stl/cstring.h"
#include <stdlib.h>

namespace fl {
    // Provide C standard library malloc/free/realloc functions
    void* malloc(size_t size) {
        return ::malloc(size);
    }

    void free(void* ptr) {
        ::free(ptr);
    }

    void* calloc(size_t nmemb, size_t size) {
        size_t total_size = nmemb * size;
        void* ptr = malloc(total_size);
        if (ptr != nullptr) {
            memset(ptr, 0, total_size);
        }
        return ptr;
    }

    void* realloc(void* ptr, size_t new_size) {
        return ::realloc(ptr, new_size);
    }

    // Provide abs function
    int abs(int x) {
        return ::abs(x);
    }
} // namespace fl