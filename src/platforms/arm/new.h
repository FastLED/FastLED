// ok no namespace fl
#pragma once

// ARM placement new operator - in global namespace
// Most ARM platforms have <new> header, handle exceptions like STM32F1

#if defined(__STM32F1__)
    // Roger Clark STM32 has broken/missing placement new
    #include "platforms/arm/inplacenew.h"
#elif !defined(__has_include)
    // Platforms without __has_include support - assume no <new> header
    #include "platforms/arm/inplacenew.h"
#elif __has_include(<new>)
    // Modern ARM platforms with standard library support
    #include <new>
#else
    // Fallback to manual definition
    #include "platforms/arm/inplacenew.h"
#endif
