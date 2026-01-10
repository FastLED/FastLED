// ok no namespace fl
#pragma once

// Shared/default placement new operator - in global namespace
// For desktop/generic platforms with standard library support

#if !defined(__has_include)
    // Platforms without __has_include support - assume no <new> header
    #include "platforms/shared/inplacenew.h"
#elif __has_include(<new>)
    // Modern platforms with standard library support
    #include <new>  // ok include // IWYU pragma: export
#elif __has_include(<new.h>)
    // Alternative standard header location
    #include <new.h>  // ok include // IWYU pragma: export
#else
    // Fallback to manual definition
    #include "platforms/shared/inplacenew.h"  // IWYU pragma: export
#endif
