#pragma once

/// @file dll.h
/// FastLED dynamic library interface - lightweight header for external callers

#ifndef FASTLED_BUILD_EXPORTS
#define FASTLED_BUILD_EXPORTS 0
#endif

#if FASTLED_BUILD_EXPORTS

#include "export.h"

#ifdef __cplusplus
extern "C" {
#endif

/// Call the sketch's setup() function
/// @note This is the C ABI export for external sketch runners
FASTLED_EXPORT void sketch_setup(void);

/// Call the sketch's loop() function  
/// @note This is the C ABI export for external sketch runners
FASTLED_EXPORT void sketch_loop(void);

#ifdef __cplusplus
}
#endif

// ================================================================================================
// IMPLEMENTATIONS (when building FastLED as shared library)
// ================================================================================================

#ifdef FASTLED_LIBRARY_SHARED

#ifdef __cplusplus
// Forward declarations - provided by sketch
extern void setup();
extern void loop();

// Provide implementations for the exported functions
FASTLED_EXPORT void sketch_setup() { 
    setup(); 
}

FASTLED_EXPORT void sketch_loop() { 
    loop(); 
}
#endif // __cplusplus

#endif // FASTLED_LIBRARY_SHARED

#endif // FASTLED_BUILD_EXPORTS
