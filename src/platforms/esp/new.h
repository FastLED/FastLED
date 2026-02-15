// ok no namespace fl
#pragma once

// IWYU pragma: private
// IWYU pragma: no_include "new"

// ESP32/ESP8266 placement new operator - in global namespace
// ESP platforms typically have <new> header available

#include "fl/has_include.h"  // IWYU pragma: keep

#if FL_HAS_INCLUDE(<new>)
    // Modern platforms with standard library support
    #include <new>  // ok include // IWYU pragma: export // IWYU pragma: keep
#elif FL_HAS_INCLUDE(<new.h>)
    // Alternative standard header location
    #include <new.h>  // ok include // IWYU pragma: export
#else
    // Fallback to manual definition
    #include "fl/stl/stdint.h"
    #include "fl/int.h"
    inline void *operator new(fl::size, void *ptr) noexcept { return ptr; }
#endif
