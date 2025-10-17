#include <malloc.h>

#include "fl/malloc.h"


namespace fl {

void* malloc(size_t size) {
   return ::malloc(size);
}

void* realloc(void* ptr, size_t size) {
   return ::realloc(ptr, size);
}

void free(void* ptr) {
   ::free(ptr);
}

}  // namespace fl
