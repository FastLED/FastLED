// Standalone translation unit for minimp3 stack/static memory auditing.

#include <stdint.h>
#if defined(__SSE2__)
#include <immintrin.h>
#endif

#include "platforms/new.h"
#include "third_party/minimp3/_build.cpp.hpp"
