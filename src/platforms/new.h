// ok no namespace fl
#pragma once

// Platform-specific placement new operator definitions
// This file dispatches to the appropriate platform-specific new.h file

// ARM platform detection
#include "platforms/arm/is_arm.h"

#if defined(ESP8266)
    #include "platforms/esp/new.h"  // IWYU pragma: export
#elif defined(ESP32)
    #include "platforms/esp/new.h"  // IWYU pragma: export
#elif defined(__AVR__)
    #include "platforms/avr/new.h"  // IWYU pragma: export
#elif defined(FASTLED_ARM)
    // All ARM platforms (Due, STM32, Teensy, nRF52, Apollo3, etc.)
    #include "platforms/arm/new.h"  // IWYU pragma: export
#elif defined(__EMSCRIPTEN__)
    // WebAssembly / Emscripten
    #include "platforms/wasm/new.h"  // IWYU pragma: export
#else
    // Default platform (desktop/generic)
    #include "platforms/shared/new.h"  // IWYU pragma: export
#endif
