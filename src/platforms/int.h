// ok no namespace fl
#pragma once

// Platform-specific integer type definitions
// This file dispatches to the appropriate platform-specific int.h file

// ARM platform detection
#include "platforms/arm/is_arm.h"

#if defined(ESP8266)
    #include "platforms/esp/int_8266.h"
#elif defined(ESP32)
    #include "platforms/esp/int.h"
#elif defined(__AVR__)
    #include "platforms/avr/int.h"
#elif defined(__IMXRT1062__)
    // Teensy 4.0/4.1 (IMXRT1062 Cortex-M7) - needs special handling for system headers
    #include "platforms/arm/teensy/teensy4_common/int.h"
#elif defined(__MK20DX128__) || defined(__MK20DX256__) || defined(__MKL26Z64__)
    // Teensy 3.x family (MK20DX/MKL26Z Cortex-M4/M0+) - needs special handling for system headers
    // - Teensy 3.0 (MK20DX128), Teensy 3.1/3.2 (MK20DX256), Teensy LC (MKL26Z64)
    #include "platforms/arm/teensy/teensy3_common/int.h"
#elif defined(FASTLED_ARM)
    // All other ARM platforms (Due, STM32, nRF52, Apollo3, etc.)
    #include "platforms/arm/int.h"
#elif defined(__EMSCRIPTEN__)
    // WebAssembly / Emscripten
    #include "platforms/wasm/int.h"
#else
    // Default platform (desktop/generic)
    #include "platforms/shared/int.h"
#endif
