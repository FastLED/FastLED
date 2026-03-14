#pragma once

// IWYU pragma: no_include "platforms/arm/is_arm.h"
// IWYU pragma: no_include "platforms/shared/int.h"

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
// This file defines:
// 1. FastLED's core integer types (fl::u8, fl::i32, etc.)
// 2. Fractional types (fract8, fract16, etc.) for LED math
// 3. Standard integer type names (uint8_t, int32_t, size_t, etc.)
//
// The 500ms compile-time savings per object file translates to significant build
// time reductions across large projects with many translation units.
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
// ⚠️  TYPEDEF CONFLICT RESOLUTION - READ THIS IF YOU GET COMPILATION ERRORS
///////////////////////////////////////////////////////////////////////////////
//
// IF YOU ENCOUNTER: "error: conflicting declaration 'typedef ...'"
// ➜ DO NOT modify this file!
// ➜ Fix must go in src/platforms/{platform}/int.h
//
// ============================================================================
// WHY TYPEDEF CONFLICTS HAPPEN
// ============================================================================
//
// This file creates standard types by wrapping FastLED's platform types:
//
//   typedef fl::u32 uint32_t;    // FastLED's definition (this file)
//   typedef __uint32_t uint32_t; // System header's definition
//
// Conflict occurs when fl::u32 ≠ __uint32_t (different base types)
// No conflict when fl::u32 = __uint32_t (identical typedefs are allowed)
//
// ============================================================================
// CONFLICT RESOLUTION PATTERN
// ============================================================================
//
// ❌ DO NOT add #ifdef guards to this file
// ❌ DO NOT change the typedef statements here
// ✅ DO modify src/platforms/{platform}/int.h
//
// EXAMPLE: ESP32 IDF 3.3 uint32_t Conflict
//
// ERROR:
//   error: conflicting declaration 'typedef fl::u32 uint32_t'
//   note: previous declaration as 'typedef __uint32_t uint32_t'
//
// SOLUTION (in src/platforms/esp/int.h):
//   Changed:  typedef __UINT32_TYPE__ u32;  // Was: unsigned int
//   To:       typedef __uint32_t u32;       // Now: matches system exactly
//
// RESULT:
//   typedef fl::u32 uint32_t → typedef __uint32_t uint32_t
//   System also has:           typedef __uint32_t uint32_t
//   Both IDENTICAL → No conflict!
//
// ============================================================================
// WHERE TO FIX CONFLICTS
// ============================================================================
//
// Platform-specific type files (modify these, not this file):
//
//   ESP32/ESP8266:     src/platforms/esp/int.h
//                      src/platforms/esp/int_8266.h
//   AVR (Arduino):     src/platforms/avr/int.h
//   ARM (general):     src/platforms/arm/int.h
//   Teensy 4.x:        src/platforms/arm/teensy/teensy4_common/int.h
//   Teensy 3.x:        src/platforms/arm/teensy/teensy3_common/int.h
//   WebAssembly:       src/platforms/wasm/int.h
//   Desktop/Generic:   src/platforms/shared/int.h
//
// Platform selection logic is in: src/platforms/int.h
//
// ============================================================================
// STEP-BY-STEP FIX GUIDE
// ============================================================================
//
// 1. Read the error message to identify conflicting type (uint32_t, size_t, etc.)
// 2. Note what base type the system uses (e.g., __uint32_t vs unsigned int)
// 3. Find your platform's int.h file from the list above
// 4. Modify the fl:: type to use the same base type as the system
// 5. Add version checks if needed (see ESP32 IDF 3.3 example above)
// 6. Test compilation
//
// ============================================================================
// RESEARCH PAST FIXES
// ============================================================================
//
//   git log --oneline --all -- 'src/platforms/**/int*.h'
//   git show 32b3c80  # ESP32 IDF 3.3 fix
//   git show 4788f81  # Teensy 4.1 fix
//
// ============================================================================
// INCLUDE CHAIN OVERVIEW
// ============================================================================
//
// User code or FastLED.h
//   └─> fl/stl/stdint.h (this file)
//       ├─> platforms/int.h (platform dispatcher)
//       │   └─> platforms/{platform}/int.h (actual type definitions)
//       ├─> Defines fl::i8, fl::u8, fractional types
//       └─> Creates global typedefs: uint8_t, size_t, etc.
//
// The key insight: global typedefs (like uint32_t) wrap fl:: types
// (like fl::u32), which are defined in platform files. If the platform
// file makes fl::u32 identical to the system's base type, the typedef
// becomes identical to the system's typedef, eliminating conflicts.
//
///////////////////////////////////////////////////////////////////////////////

// Platform-specific integer type definitions
// This includes platform-specific 16/32/64-bit types
#include "platforms/int.h"  // IWYU pragma: keep

namespace fl {
    // 8-bit types - char is reliably 8 bits on all supported platforms
    typedef signed char i8;
    typedef unsigned char u8;
    typedef unsigned int uint;

    // Pointer and size types are defined per-platform in platforms/int.h
    // uptr (pointer type) and size (size type) are defined per-platform
}

namespace fl {
    ///////////////////////////////////////////////////////////////////////
    ///
    /// Fixed-Point Fractional Types.
    /// Types for storing fractional data.
    ///
    /// * ::sfract7 should be interpreted as signed 128ths.
    /// * ::fract8 should be interpreted as unsigned 256ths.
    /// * ::sfract15 should be interpreted as signed 32768ths.
    /// * ::fract16 should be interpreted as unsigned 65536ths.
    ///
    /// Example: if a fract8 has the value "64", that should be interpreted
    ///          as 64/256ths, or one-quarter.
    ///
    /// accumXY types should be interpreted as X bits of integer,
    ///         and Y bits of fraction.
    /// E.g., ::accum88 has 8 bits of int, 8 bits of fraction
    ///

    /// ANSI: unsigned short _Fract.
    /// Range is 0 to 0.99609375 in steps of 0.00390625.
    /// Should be interpreted as unsigned 256ths.
    typedef u8   fract8;

    /// ANSI: signed short _Fract.
    /// Range is -0.9921875 to 0.9921875 in steps of 0.0078125.
    /// Should be interpreted as signed 128ths.
    typedef i8    sfract7;

    /// ANSI: unsigned _Fract.
    /// Range is 0 to 0.99998474121 in steps of 0.00001525878.
    /// Should be interpreted as unsigned 65536ths.
    typedef u16  fract16;

    typedef i32   sfract31; ///< ANSI: signed long _Fract. 31 bits int, 1 bit fraction

    typedef u32  fract32;   ///< ANSI: unsigned long _Fract. 32 bits int, 32 bits fraction

    /// ANSI: signed _Fract.
    /// Range is -0.99996948242 to 0.99996948242 in steps of 0.00003051757.
    /// Should be interpreted as signed 32768ths.
    typedef i16   sfract15;

    typedef u16  accum88;    ///< ANSI: unsigned short _Accum. 8 bits int, 8 bits fraction
    typedef i16   saccum78;   ///< ANSI: signed   short _Accum. 7 bits int, 8 bits fraction
    typedef u32  accum1616;  ///< ANSI: signed         _Accum. 16 bits int, 16 bits fraction
    typedef i32   saccum1516; ///< ANSI: signed         _Accum. 15 bits int, 16 bits fraction
    typedef u16  accum124;   ///< no direct ANSI counterpart. 12 bits int, 4 bits fraction
    typedef i32   saccum114;  ///< no direct ANSI counterpart. 1 bit int, 14 bits fraction
}

// Size assertions moved to src/platforms/compile_test.cpp.hpp

// Make fractional types available in global namespace
using fl::fract8;      // ok bare using
using fl::sfract7;     // ok bare using
using fl::fract16;     // ok bare using
using fl::sfract31;    // ok bare using
using fl::fract32;     // ok bare using
using fl::sfract15;    // ok bare using
using fl::accum88;     // ok bare using
using fl::saccum78;    // ok bare using
using fl::accum1616;   // ok bare using
using fl::saccum1516;  // ok bare using
using fl::accum124;    // ok bare using
using fl::saccum114;   // ok bare using

// Define standard integer type names for C++
// This avoids the slow <stdint.h> include while maintaining compatibility
// 8-bit types use raw primitives to match system headers exactly (allows duplicate typedefs)
typedef unsigned char uint8_t;
typedef signed char int8_t;

// 16-bit types use platform-specific fl:: types to handle platform differences
// (AVR: int is 16-bit, most others: short is 16-bit)
typedef fl::u16 uint16_t;
typedef fl::i16 int16_t;

// Define standard types using fl:: types from platform-specific int.h
// This ensures we match the platform's type sizes correctly
typedef fl::u32 uint32_t;
typedef fl::i32 int32_t;
typedef fl::u64 u64;
typedef fl::i64 i64;
typedef fl::size size_t;
typedef fl::uptr uintptr_t;
typedef fl::iptr intptr_t;
typedef fl::ptrdiff ptrdiff_t;

