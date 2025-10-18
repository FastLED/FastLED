#pragma once

#include "fl/stdint.h"

namespace fl {
   void* malloc(size_t size);
   void* calloc(size_t nmemb, size_t size);
   void* realloc(void* ptr, size_t size);
   void free(void* ptr);
}  // namespace fl
