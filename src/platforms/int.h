#pragma once

// Platform-specific integer type definitions
// This file dispatches to the appropriate platform-specific int.h file

#include "platforms/arm/is_arm.h"


#if defined(ESP32)
    #include "platforms/esp/int.h"
#elif defined(__AVR__)
    #include "platforms/avr/int.h"
#elif defined(FASTLED_ARM)
    // All ARM platforms (Due, Teensy, STM32, nRF52, Apollo3, etc.)
    #include "platforms/arm/int.h"
#elif defined(__EMSCRIPTEN__)
    // WebAssembly / Emscripten
    #include "platforms/wasm/int.h"
#else
    // Default platform (desktop/generic)
    #include "platforms/shared/int.h"
#endif
