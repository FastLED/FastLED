// ok no namespace fl
#pragma once

// Platform-specific placement new operator definitions
// This file dispatches to the appropriate platform-specific new.h file

// ARM platform detection
#include "platforms/arm/is_arm.h"

#if defined(ESP8266)
    #include "platforms/esp/new.h"
#elif defined(ESP32)
    #include "platforms/esp/new.h"
#elif defined(__AVR__)
    #include "platforms/avr/new.h"
#elif defined(FASTLED_ARM)
    // All ARM platforms (Due, STM32, Teensy, nRF52, Apollo3, etc.)
    #include "platforms/arm/new.h"
#elif defined(__EMSCRIPTEN__)
    // WebAssembly / Emscripten
    #include "platforms/wasm/new.h"
#else
    // Default platform (desktop/generic)
    #include "platforms/shared/new.h"
#endif
