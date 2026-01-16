#pragma once

/// @file thread_config.h
/// @brief Platform detection for FASTLED_MULTITHREADED macro
///
/// This header ONLY defines the FASTLED_MULTITHREADED macro based on platform.
/// It does NOT include any threading implementation to avoid circular dependencies.
/// Use this header when you only need to check if multithreading is enabled.

#include "fl/has_include.h"
#include "platforms/is_platform.h"

// Platform-specific FASTLED_MULTITHREADED detection
// Each platform defines this based on its threading capabilities

#ifndef FASTLED_MULTITHREADED

#if defined(FL_IS_STUB) || defined(FL_IS_WASM)
    // Stub/WASM: Enable if pthread available
    #if FL_HAS_INCLUDE(<pthread.h>)
        #define FASTLED_MULTITHREADED 1
    #else
        #define FASTLED_MULTITHREADED 0
    #endif
#elif defined(FL_IS_ESP32)
    // ESP32: FreeRTOS provides threading
    #define FASTLED_MULTITHREADED 1
#elif defined(FASTLED_TESTING) && FL_HAS_INCLUDE(<pthread.h>)
    // Testing builds with pthread
    #define FASTLED_MULTITHREADED 1
#else
    // Default: No threading support (AVR, STM32, Teensy, etc.)
    #define FASTLED_MULTITHREADED 0
#endif

#endif  // FASTLED_MULTITHREADED
