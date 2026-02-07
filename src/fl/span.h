#pragma once

// DEPRECATED: This file location is deprecated
// Use "fl/stl/span.h" instead of "fl/span.h"
// This stub exists for backward compatibility only

#include "platforms/is_platform.h"

#if defined(FL_IS_GCC) || defined(FL_IS_CLANG)
#warning "fl/span.h is deprecated, use fl/stl/span.h instead"
#endif

#include "fl/stl/span.h"
