// ok no namespace fl
#pragma once

// ESP32/ESP8266 placement new operator - in global namespace
// ESP platforms typically have <new> header available

#if __has_include(<new>)
    #include <new>
#else
    // Fallback: manual placement new definition
    #include "platforms/esp/inplacenew.h"
#endif
