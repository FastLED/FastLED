#include "malloc.h"
#include <stdlib.h>

namespace fl {
    // Provide C standard library malloc/free/realloc functions
    void* malloc(size_t size) {
        return ::malloc(size);
    }

    void free(void* ptr) {
        ::free(ptr);
    }

    void* realloc(void* ptr, size_t new_size) {
        return ::realloc(ptr, new_size);
    }

    // Provide abs function
    int abs(int x) {
        return ::abs(x);
    }
} // namespace fl