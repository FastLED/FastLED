// ok no namespace fl
#pragma once
#include "teensy/is_teensy.h"

// ARM placement new operator - in global namespace
// Most ARM platforms have <new> header available

// Teensy 3.x/LC compatibility: Include new.h early to declare __cxa_guard_* functions
// before any function-local statics are seen by the compiler
#if defined(FL_IS_TEENSY_3X) || defined(FL_IS_TEENSY_LC)
    #include <new.h>  // ok include // IWYU pragma: export
#endif

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
