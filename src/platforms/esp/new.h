// ok no namespace fl
#pragma once

// ESP32/ESP8266 placement new operator - in global namespace
// ESP platforms typically have <new> header available

#if !defined(__has_include)
    // Platforms without __has_include support - assume no <new> header
    #include "platforms/esp/inplacenew.h"
#elif __has_include(<new>)
    // Modern platforms with standard library support
    #include <new>  // ok include
#elif __has_include(<new.h>)
    // Alternative standard header location
    #include <new.h>  // ok include
#else
    // Fallback to manual definition
    #include "platforms/esp/inplacenew.h"
#endif
