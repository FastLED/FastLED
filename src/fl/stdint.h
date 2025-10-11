#pragma once

///////////////////////////////////////////////////////////////////////////////
// FastLED Integer Type Definitions
//
// IMPORTANT: FastLED has carefully purged <stdint.h> and <stddef.h> from the
// include path because including those headers alone adds approximately 500ms
// to the compilation time of EVERY *.o file.
//
// Instead, FastLED defines its own integer types (fl::u8, fl::u16, fl::i32, etc.)
// using primitive types (char, short, int, long, long long) in platform-specific
// int.h files. These types are guaranteed to match stdint.h types exactly, and
// this is enforced by compile-time tests in platforms/compile_test.cpp.
//
// This file provides standard integer type names (uint8_t, int32_t, size_t, etc.)
// as typedefs of FastLED types, maintaining compatibility while avoiding the
// slow standard library headers.
//
// The 500ms compile-time savings per object file translates to significant build
// time reductions across large projects with many translation units.
///////////////////////////////////////////////////////////////////////////////

// Include fl/int.h to get FastLED integer types (u8, u16, i8, etc.)
// This defines types using primitive types which compiles faster than <stdint.h>
#include "fl/int.h"
#include "fl/cstddef.h"

// On Arduino platforms, the system headers will define these types
// We must use the system definitions to avoid conflicts
#if defined(ARDUINO) && !defined(__EMSCRIPTEN__)
    // Arduino platform - use system stdint.h
    #include <stdint.h>  // ok include - required for Arduino compatibility
#else
    // Non-Arduino platforms - define our own types for faster compilation
    // IMPORTANT: Only define these if system headers haven't already defined them
    //            Guard against redefinition conflicts with system <stdint.h>
    
    // Check if system stdint.h has already been included
    // If so, skip our typedefs to avoid conflicts
    #if !defined(_STDINT_H) && !defined(_STDINT_H_) && !defined(_STDINT) && !defined(__STDINT_H) && !defined(_GCC_STDINT_H) && !defined(_GCC_WRAP_STDINT_H)
    
    // Only define basic integer types if system headers haven't
    typedef unsigned char uint8_t;
    typedef signed char int8_t;
    typedef unsigned short uint16_t;
    typedef short int16_t;
    typedef fl::u32 uint32_t;
    typedef fl::i32 int32_t;
    typedef fl::u64 uint64_t;
    typedef fl::i64 int64_t;
    
    #endif // !defined(_STDINT_H) && ...
#endif // defined(ARDUINO)

// Define size_t, uintptr_t, ptrdiff_t using fl:: types
// Only define these if not already defined by system headers
// On Arduino platforms, these are already defined by system headers

#if !defined(ARDUINO) || defined(__EMSCRIPTEN__)
    // Non-Arduino platforms - define our own types
    #ifndef _SIZE_T_DEFINED
    typedef fl::size size_t;
    #define _SIZE_T_DEFINED
    #endif
    
    #ifndef _UINTPTR_T_DEFINED  
    typedef fl::uptr uintptr_t;
    #define _UINTPTR_T_DEFINED
    #endif
    
    #ifndef _PTRDIFF_T_DEFINED
    typedef fl::ptrdiff ptrdiff_t;
    #define _PTRDIFF_T_DEFINED
    #endif
#endif // !defined(ARDUINO) || defined(__EMSCRIPTEN__)

// stdint.h limit macros
// These match the standard stdint.h definitions
// Guard against redefinition if system headers already defined them
#ifndef INT8_MIN
#define INT8_MIN   (-128)
#endif
#ifndef INT16_MIN
#define INT16_MIN  (-32767-1)
#endif
#ifndef INT32_MIN
#define INT32_MIN  (-2147483647-1)
#endif
#ifndef INT64_MIN
#define INT64_MIN  (-9223372036854775807LL-1)
#endif

#ifndef INT8_MAX
#define INT8_MAX   127
#endif
#ifndef INT16_MAX
#define INT16_MAX  32767
#endif
#ifndef INT32_MAX
#define INT32_MAX  2147483647
#endif
#ifndef INT64_MAX
#define INT64_MAX  9223372036854775807LL
#endif

#ifndef UINT8_MAX
#define UINT8_MAX  0xFF
#endif
#ifndef UINT16_MAX
#define UINT16_MAX 0xFFFF
#endif
#ifndef UINT32_MAX
#define UINT32_MAX 0xFFFFFFFFU
#endif
#ifndef UINT64_MAX
#define UINT64_MAX 0xFFFFFFFFFFFFFFFFULL
#endif
