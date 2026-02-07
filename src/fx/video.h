#pragma once

// Trampoline header: video.h has moved to fl/fx/video.h
// This header exists for backward compatibility and will be removed in a future version.

#include "platforms/is_platform.h"

#if defined(FL_IS_GCC) || defined(FL_IS_CLANG)
#warning "fx/video.h is deprecated, use fl/fx/video.h instead"
#endif

#include "fl/fx/video.h"
