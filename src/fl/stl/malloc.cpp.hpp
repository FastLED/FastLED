#include "fl/stl/malloc.h"
#include "fl/stl/cstring.h"
// IWYU pragma: begin_keep
#include <stdlib.h>
// IWYU pragma: end_keep

namespace fl {
    // Provide C standard library malloc/free/realloc functions
    void* malloc(size_t size) FL_NO_EXCEPT {
        return ::malloc(size);
    }

    void free(void* ptr) FL_NO_EXCEPT {
        ::free(ptr);
    }

    void* calloc(size_t nmemb, size_t size) FL_NO_EXCEPT {
        size_t total_size = nmemb * size;
        void* ptr = malloc(total_size);
        if (ptr != nullptr) {
            memset(ptr, 0, total_size);
        }
        return ptr;
    }

    void* realloc(void* ptr, size_t new_size) FL_NO_EXCEPT {
        return ::realloc(ptr, new_size);
    }

    // Provide abs function
    // Arduino.h defines abs as a macro, so we need to temporarily hide it
    #pragma push_macro("abs")
    #undef abs
    int abs(int x) {
        return ::abs(x);
    }
    #pragma pop_macro("abs")
} // namespace fl