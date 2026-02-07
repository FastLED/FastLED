#pragma once

// DEPRECATED: This file location is deprecated
// Use "fl/stl/cstring.h" instead of "fl/cstring.h"
// This stub exists for backward compatibility only

#include "platforms/is_platform.h"

#if defined(FL_IS_GCC) || defined(FL_IS_CLANG)
#warning "fl/cstring.h is deprecated, use fl/stl/cstring.h instead"
#endif

#include "fl/stl/cstring.h"
