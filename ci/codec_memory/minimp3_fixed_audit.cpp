// Standalone translation unit for fixed-point minimp3 stack/static memory
// auditing.
//
// The sibling minimp3_audit.cpp pulls in the component's unity build, which
// compiles the float decoder. The fixed-point path is the same header with
// MINIMP3_FIXED_POINT defined, so this TU instantiates it directly rather than
// through _build.cpp.hpp -- the two cannot share a unity build without
// colliding, and keeping them in separate objects is what lets the audit report
// their stack and table footprints independently.

#include <stdint.h>
#if defined(__SSE2__)
#include <immintrin.h>
#endif

#include "platforms/new.h"

#define MINIMP3_FIXED_POINT 1
#define MINIMP3_IMPLEMENTATION
#define MINIMP3_NO_STDIO
#include "third_party/minimp3/minimp3.h" // ok cpp include
#undef MINIMP3_NO_STDIO
#undef MINIMP3_IMPLEMENTATION
#undef MINIMP3_FIXED_POINT
