// Standalone translation unit for minimp3 stack/static memory auditing.
//
// The float variant, pinned. minimp3.h defaults to the fixed-point DSP now that
// it is the production path, so an audit TU that relied on the default would
// silently start measuring the other build and compare it against
// float-labelled ledger rows. The sibling minimp3_fixed_audit.cpp pins the
// other direction for the same reason.

#include <stdint.h>
#if defined(__SSE2__)
#include <immintrin.h>
#endif

#include "platforms/new.h"

#define MINIMP3_FLOAT_POINT 1
#include "third_party/minimp3/_build.cpp.hpp"
#undef MINIMP3_FLOAT_POINT
