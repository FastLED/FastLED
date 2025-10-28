// ok no namespace fl
#pragma once

// ARM placement new operator - in global namespace
// Most ARM platforms have <new> header available

#if !defined(__has_include)
    // Platforms without __has_include support - assume no <new> header
    #include "platforms/arm/inplacenew.h"
#elif __has_include(<new>)
    // Modern ARM platforms with standard library support (includes STM32F1 with modern toolchains)
    #include <new>  // ok include
#elif __has_include(<new.h>)
    // Alternative standard header location
    #include <new.h> // ok include
#else
    // Fallback to manual definition for platforms without <new> header
    #include "platforms/arm/inplacenew.h"
#endif
