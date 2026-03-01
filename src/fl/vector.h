#pragma once

// DEPRECATED: This file location is deprecated
// Use "fl/stl/vector.h" instead of "fl/vector.h"
// This stub exists for backward compatibility only

#include "platforms/is_platform.h"  // IWYU pragma: keep

#if defined(FL_IS_GCC) || defined(FL_IS_CLANG)
#warning "fl/vector.h is deprecated, use fl/stl/vector.h instead"
#endif
