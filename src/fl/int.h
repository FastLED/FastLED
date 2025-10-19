#pragma once

///////////////////////////////////////////////////////////////////////////////
// PLATFORM-SPECIFIC INTEGER TYPE CONFLICT RESOLUTION GUIDE
//
// ⚠️  CRITICAL: When integer type conflicts occur (typedef redefinition errors),
//              the fix MUST go in src/platforms/**/int*.h files.
//              DO NOT modify this file (fl/int.h) or fl/stdint.h!
//
// ============================================================================
// WHAT THIS FILE DOES
// ============================================================================
// This file defines FastLED's core integer types (fl::u8, fl::i32, etc.) by:
// 1. Including platforms/int.h (which selects the correct platform file)
// 2. Defining common 8-bit types (i8/u8) used across all platforms
// 3. Defining fractional types (fract8, fract16, etc.) for LED math
//
// Platform-specific types (16/32/64-bit, pointer types) are defined in:
//   platforms/{platform}/int.h
//
// ============================================================================
// WHEN TYPEDEF CONFLICTS OCCUR
// ============================================================================
//
// EXAMPLE COMPILATION ERROR:
//   error: conflicting declaration 'typedef fl::u32 uint32_t'
//   note: previous declaration as 'typedef __uint32_t uint32_t'
//
// This happens when:
// 1. System header (Arduino.h, stdint.h) defines: typedef __uint32_t uint32_t
// 2. FastLED's fl/stdint.h defines: typedef fl::u32 uint32_t
// 3. fl::u32 != __uint32_t (they're different base types)
//
// ============================================================================
// HOW TO FIX TYPEDEF CONFLICTS
// ============================================================================
//
// ❌ DO NOT modify fl/int.h (this file)
// ❌ DO NOT modify fl/stdint.h
// ❌ DO NOT add #ifdef guards around typedefs
// ✅ DO modify the platform-specific file in src/platforms/**/int.h
//
// SOLUTION PATTERN:
// 1. Identify which type is conflicting (e.g., uint32_t)
// 2. Find which platform file defines the fl:: equivalent (e.g., fl::u32)
// 3. Change the fl:: type to use the SAME base type as the system header
//
// ============================================================================
// WORKED EXAMPLE: ESP32 IDF 3.3 uint32_t Conflict
// ============================================================================
//
// PROBLEM:
//   System stdint.h: typedef __uint32_t uint32_t;
//   fl/stdint.h:     typedef fl::u32 uint32_t;
//   platforms/esp/int.h: typedef __UINT32_TYPE__ u32;  // expands to 'unsigned int'
//   Result: __uint32_t != unsigned int → CONFLICT!
//
// SOLUTION (in src/platforms/esp/int.h):
//   Before:
//     typedef __UINT32_TYPE__ u32;  // Compiler builtin, expands to 'unsigned int'
//
//   After:
//     #if !defined(ESP_IDF_VERSION) || !ESP_IDF_VERSION_4_OR_HIGHER
//       // IDF < 4.0: Use system's base type to match stdint.h
//       typedef __uint32_t u32;     // Now fl::u32 = __uint32_t
//     #else
//       typedef __UINT32_TYPE__ u32;  // IDF >= 4.0: Use compiler builtin
//     #endif
//
// WHY THIS WORKS:
//   fl/stdint.h:     typedef fl::u32 uint32_t;
//   Since fl::u32 = __uint32_t, this becomes:
//                    typedef __uint32_t uint32_t;
//   System header:   typedef __uint32_t uint32_t;
//   Both typedefs are IDENTICAL → No conflict! (C++ allows duplicate identical typedefs)
//
// ============================================================================
// WORKED EXAMPLE: Teensy 4.1 size_t Conflict
// ============================================================================
//
// PROBLEM:
//   System stddef.h: typedef unsigned int size_t;
//   fl/stdint.h:     typedef fl::size size_t;
//   platforms/arm/int.h: typedef unsigned long size;  // Wrong base type!
//   Result: unsigned int != unsigned long → CONFLICT!
//
// SOLUTION:
//   Created new platform-specific file: src/platforms/arm/teensy/teensy4_common/int.h
//   typedef unsigned int size;  // Matches Teensy 4.x system headers exactly
//
// RESULT:
//   fl/stdint.h:     typedef fl::size size_t;
//   Since fl::size = unsigned int, this becomes:
//                    typedef unsigned int size_t;
//   System header:   typedef unsigned int size_t;
//   Both typedefs are IDENTICAL → No conflict!
//
// ============================================================================
// PLATFORM FILE LOCATIONS
// ============================================================================
//
// When fixing conflicts, modify these files:
//
//   ESP32/ESP8266:     src/platforms/esp/int.h
//                      src/platforms/esp/int_8266.h (ESP8266 specific)
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
// TROUBLESHOOTING CHECKLIST
// ============================================================================
//
// When you encounter a typedef conflict:
//
// □ Step 1: Identify the conflicting type (e.g., uint32_t, size_t, uintptr_t)
// □ Step 2: Find which system header defines it (look at error messages)
// □ Step 3: Determine what base type the system uses (e.g., __uint32_t vs unsigned int)
// □ Step 4: Find the platform-specific int.h file for your platform
// □ Step 5: Modify the fl:: type to use the same base type as the system
// □ Step 6: If needed, add version/platform detection (see ESP32 IDF 3.3 example)
// □ Step 7: Apply changes to BOTH C++ (namespace fl) and C sections
// □ Step 8: Test compilation on the affected platform
//
// ============================================================================
// HOW TO RESEARCH PAST FIXES
// ============================================================================
//
// To learn from previous fixes:
//   git log --oneline --all -- 'src/platforms/**/int*.h'
//   git log -p --all -- 'src/platforms/**/int*.h'
//   git log --grep="typedef\|stdint\|int32_t" -- src/platforms/
//   git show 32b3c80  # ESP32 IDF 3.3 fix
//   git show 4788f81  # Teensy 4.1 fix
//
// ============================================================================
// INCLUDE CHAIN OVERVIEW
// ============================================================================
//
// User code or FastLED.h
//   └─> fl/stdint.h (standard type compatibility layer)
//       ├─> fl/int.h (this file - platform-agnostic interface)
//       │   ├─> platforms/int.h (platform dispatcher)
//       │   │   └─> platforms/{platform}/int.h (actual type definitions)
//       │   └─> Defines i8, u8, fractional types
//       └─> Creates global typedefs: uint32_t, size_t, etc.
//
// The key insight: fl/stdint.h typedefs (like uint32_t) wrap fl:: types
// (like fl::u32), which are defined in platform files. If the platform
// file makes fl::u32 identical to the system's base type, the typedef
// becomes identical to the system's typedef, eliminating conflicts.
//
///////////////////////////////////////////////////////////////////////////////

/// IMPORTANT!
/// This file MUST NOT include "fl/stdint.h" to prevent circular inclusion.
/// fl/stdint.h includes this file.

// Platform-specific integer type definitions
// This includes platform-specific 16/32/64-bit types
#include "platforms/int.h"

#ifdef __cplusplus
namespace fl {
    // 8-bit types - char is reliably 8 bits on all supported platforms
    // These must be defined BEFORE platform includes so fractional types can use them
    typedef signed char i8;
    typedef unsigned char u8;
    typedef unsigned int uint;
    
    // Pointer and size types are defined per-platform in platforms/int.h
    // uptr (pointer type) and size (size type) are defined per-platform

}
#else
// C language compatibility - define types in global namespace
typedef signed char i8;
typedef unsigned char u8;
typedef unsigned int uint;
#endif


#ifdef __cplusplus
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
using fl::fract8;
using fl::sfract7;
using fl::fract16;
using fl::sfract31;
using fl::fract32;
using fl::sfract15;
using fl::accum88;
using fl::saccum78;
using fl::accum1616;
using fl::saccum1516;
using fl::accum124;
using fl::saccum114;
#else
// C language compatibility - define fractional types in global namespace
typedef u8   fract8;
typedef i8   sfract7;
typedef u16  fract16;
typedef i32  sfract31;
typedef u32  fract32;
typedef i16  sfract15;
typedef u16  accum88;
typedef i16  saccum78;
typedef u32  accum1616;
typedef i32  saccum1516;
typedef u16  accum124;
typedef i32  saccum114;
#endif
