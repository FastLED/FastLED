#pragma once

// Platform-specific integer type definitions
// This file dispatches to the appropriate platform-specific int.h file

#include "platforms/arm/is_arm.h"


#if defined(ESP8266)
    #include "platforms/esp/int_8266.h"
#elif defined(ESP32)
    #include "platforms/esp/int.h"
#elif defined(__AVR__)
    #include "platforms/avr/int.h"
#elif defined(__IMXRT1062__)
    // Teensy 4.0/4.1 (IMXRT1062 Cortex-M7) - needs special handling for system headers
    #include "platforms/arm/mxrt1062/int.h"
#elif defined(FASTLED_ARM)
    // All other ARM platforms (Due, Teensy 3.x, STM32, nRF52, Apollo3, etc.)
    #include "platforms/arm/int.h"
#elif defined(__EMSCRIPTEN__)
    // WebAssembly / Emscripten
    #include "platforms/wasm/int.h"
#else
    // Default platform (desktop/generic)
    #include "platforms/shared/int.h"
#endif
