#include "fl/stl/malloc.h"
#include "fl/stl/cstring.h"
// IWYU pragma: begin_keep
#include <stdlib.h>
#include "fl/stl/noexcept.h"
// IWYU pragma: end_keep

namespace fl {
    // Provide C standard library malloc/free/realloc functions
    void* libc_impl::malloc(size_t size) FL_NO_EXCEPT {
        return ::malloc(size);
    }

    void libc_impl::free(void* ptr) FL_NO_EXCEPT {
        ::free(ptr);
    }

    void* libc_impl::calloc(size_t nmemb, size_t size) FL_NO_EXCEPT {
        size_t total_size = nmemb * size;
        void* ptr = malloc(total_size);
        if (ptr != nullptr) {
            memset(ptr, 0, total_size);
        }
        return ptr;
    }

    void* libc_impl::realloc(void* ptr, size_t new_size) FL_NO_EXCEPT {
        return ::realloc(ptr, new_size);
    }
} // namespace fl