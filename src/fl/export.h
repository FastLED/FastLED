#pragma once

/// @file export.h
/// Cross-platform export macros for FastLED dynamic library support

#ifndef FASTLED_EXPORT
    #if defined(__EMSCRIPTEN__)
        #include <emscripten.h>
        #define FASTLED_EXPORT EMSCRIPTEN_KEEPALIVE
    #elif defined(_WIN32) || defined(_WIN64)
        // Windows DLL export/import
        #ifdef FASTLED_BUILDING_DLL
            #define FASTLED_EXPORT __declspec(dllexport)
        #else
            #define FASTLED_EXPORT __declspec(dllimport)
        #endif
    #elif defined(__GNUC__) && (__GNUC__ >= 4)
        // GCC/Clang visibility attributes
        #define FASTLED_EXPORT __attribute__((visibility("default")))
    #elif defined(__SUNPRO_CC) && (__SUNPRO_CC >= 0x550)
        // Sun Studio visibility attributes
        #define FASTLED_EXPORT __global
    #else
        // Fallback for other platforms
        #define FASTLED_EXPORT
    #endif
#endif

#ifndef FASTLED_CALL
    #if defined(_WIN32) || defined(_WIN64)
        // Windows calling convention
        #define FASTLED_CALL __stdcall
    #else
        // Unix-like platforms - no special calling convention
        #define FASTLED_CALL
    #endif
#endif

/// Combined export and calling convention macro
#define FASTLED_API FASTLED_EXPORT FASTLED_CALL
