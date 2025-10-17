#if defined(__APPLE__)
#include <cstdlib>
#else
#include <malloc.h>
#endif

#include "fl/malloc.h"


namespace fl {

void* malloc(size_t size) {
#if defined(__APPLE__)
   return std::malloc(size);
#else
   return ::malloc(size);
#endif
}

void* realloc(void* ptr, size_t size) {
#if defined(__APPLE__)
   return std::realloc(ptr, size);
#else
   return ::realloc(ptr, size);
#endif
}

void free(void* ptr) {
#if defined(__APPLE__)
   std::free(ptr);
#else
   ::free(ptr);
#endif
}

}  // namespace fl
