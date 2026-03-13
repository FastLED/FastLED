// ok no namespace fl
#pragma once

// IWYU pragma: private
// IWYU pragma: no_include "new"

// Shared/default placement new operator - in global namespace
// For desktop/generic platforms with standard library support

#include "fl/stl/has_include.h"  // IWYU pragma: keep

#if FL_HAS_INCLUDE(<new>)
    // Modern platforms with standard library support
    #include <new>  // ok include // IWYU pragma: export // IWYU pragma: keep
#elif FL_HAS_INCLUDE(<new.h>)
    // Alternative standard header location
    #include <new.h>  // ok include // IWYU pragma: export // IWYU pragma: keep
#else
    // Fallback to manual definition
    #include "fl/stl/stdint.h"
    #include "fl/stl/int.h"
    inline void *operator new(fl::size, void *ptr) noexcept { return ptr; }
#endif
