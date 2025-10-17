// fl_malloc.cpp  (implementation file)
// C++11, AVR- and bare-metal-friendly


#include <stdlib.h>  // ::malloc, ::realloc, ::free

#include "fl/malloc.h"

namespace fl {

void* malloc(size_t size) noexcept {
    return ::malloc(size);
}

void* realloc(void* ptr, size_t size) noexcept {
    return ::realloc(ptr, size);
}

void free(void* ptr) noexcept {
    ::free(ptr);
}

}  // namespace fl
