// ok no namespace fl
#pragma once

// ARM placement new operator - in global namespace
// Most ARM platforms have <new> header available

#if !defined(__has_include)
    // Platforms without __has_include support - define placement new manually
    #include "fl/stl/stdint.h"
    #include "fl/int.h"
    inline void *operator new(fl::size, void *ptr) noexcept { return ptr; }
#elif __has_include(<new>)
    // Modern ARM platforms with standard library support (includes STM32F1 with modern toolchains)
    #include <new>  // ok include // IWYU pragma: export
#elif __has_include(<new.h>)
    // Alternative standard header location
    #include <new.h> // ok include // IWYU pragma: export
#else
    // Fallback to manual definition for platforms without <new> header
    #include "fl/stl/stdint.h"
    #include "fl/int.h"
    inline void *operator new(fl::size, void *ptr) noexcept { return ptr; }
#endif
