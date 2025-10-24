// ok no namespace fl
#pragma once

// Platform-specific atomic operations
// This file dispatches to the appropriate platform-specific atomic.h file
// Pattern follows platforms/int.h

#include "platforms/arm/is_arm.h"

// For now, all platforms use the GCC/Clang __atomic intrinsics implementation
// Future platform-specific implementations can be added here as needed:
// - platforms/wasm/atomic.h - if WASM needs different atomics
// - platforms/win/atomic_msvc.h - if pure MSVC support is needed
// - platforms/avr/atomic.h - if AVR needs optimized atomics

#if defined(ESP32)
    // ESP32 uses GCC, so shared implementation works
    #include "platforms/shared/atomic.h"
#elif defined(__AVR__)
    // AVR-GCC supports __atomic builtins
    #include "platforms/shared/atomic.h"
#elif defined(FASTLED_ARM)
    // ARM GCC supports __atomic builtins
    #include "platforms/shared/atomic.h"
#elif defined(__EMSCRIPTEN__)
    // Emscripten/Clang supports __atomic builtins
    #include "platforms/shared/atomic.h"
#else
    // Default platform (desktop/generic): GCC/Clang/MinGW-W64
    #include "platforms/shared/atomic.h"
#endif
