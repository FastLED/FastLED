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

///////////////////////////////////////////////////////////////////////////////
// ⚠️  TYPEDEF CONFLICT RESOLUTION - READ THIS IF YOU GET COMPILATION ERRORS
///////////////////////////////////////////////////////////////////////////////
//
// IF YOU ENCOUNTER: "error: conflicting declaration 'typedef ...'"
// ➜ DO NOT modify this file!
// ➜ See fl/int.h for the complete troubleshooting guide
// ➜ Summary: Fix must go in src/platforms/{platform}/int.h
//
// ============================================================================
// WHY TYPEDEF CONFLICTS HAPPEN
// ============================================================================
//
// This file creates standard C/C++ types by wrapping FastLED's platform types:
//
//   typedef fl::u32 uint32_t;    // FastLED's definition (this file)
//   typedef __uint32_t uint32_t; // System header's definition
//
// Conflict occurs when fl::u32 ≠ __uint32_t (different base types)
// No conflict when fl::u32 = __uint32_t (identical typedefs are allowed)
//
// ============================================================================
// HOW THIS FILE WORKS
// ============================================================================
//
// 1. Includes fl/int.h (which includes platform-specific type definitions)
// 2. Creates standard type names as typedefs of fl:: types:
//      typedef fl::u32 uint32_t;
//      typedef fl::size size_t;
//      typedef fl::uptr uintptr_t;
//
// The typedefs below are INTENTIONALLY simple and should NEVER be modified.
// They create a clean mapping from fl:: types to standard names.
//
// ============================================================================
// CONFLICT RESOLUTION PATTERN
// ============================================================================
//
// ❌ DO NOT add #ifdef guards to this file
// ❌ DO NOT change the typedef statements here
// ❌ DO NOT modify fl/int.h
// ✅ DO modify src/platforms/{platform}/int.h
//
// EXAMPLE: ESP32 IDF 3.3 uint32_t Conflict
//
// ERROR:
//   error: conflicting declaration 'typedef fl::u32 uint32_t'
//   note: previous declaration as 'typedef __uint32_t uint32_t'
//
// CAUSE:
//   System header: typedef __uint32_t uint32_t;
//   This file:     typedef fl::u32 uint32_t;
//   Problem:       fl::u32 was defined as __UINT32_TYPE__ (= unsigned int)
//                  but system uses __uint32_t (different type)
//
// SOLUTION (in src/platforms/esp/int.h):
//   Changed:  typedef __UINT32_TYPE__ u32;  // Was: unsigned int
//   To:       typedef __uint32_t u32;       // Now: matches system exactly
//
// RESULT:
//   This file:     typedef fl::u32 uint32_t;
//   Expands to:    typedef __uint32_t uint32_t;  // fl::u32 = __uint32_t
//   System header: typedef __uint32_t uint32_t;
//   Both typedefs are IDENTICAL → No conflict!
//
// ============================================================================
// EXAMPLE: Teensy 4.1 size_t Conflict
// ============================================================================
//
// ERROR:
//   error: conflicting declaration 'typedef fl::size size_t'
//   note: previous declaration as 'typedef unsigned int size_t'
//
// CAUSE:
//   System header: typedef unsigned int size_t;
//   This file:     typedef fl::size size_t;
//   Problem:       fl::size was defined as unsigned long (wrong!)
//
// SOLUTION:
//   Created platform-specific file: src/platforms/arm/teensy/teensy4_common/int.h
//   Changed:  typedef unsigned long size;   // Was: wrong type
//   To:       typedef unsigned int size;    // Now: matches Teensy 4.x system headers
//
// RESULT:
//   This file:     typedef fl::size size_t;
//   Expands to:    typedef unsigned int size_t;  // fl::size = unsigned int
//   System header: typedef unsigned int size_t;
//   Both typedefs are IDENTICAL → No conflict!
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
// ============================================================================
// STEP-BY-STEP FIX GUIDE
// ============================================================================
//
// 1. Read the error message to identify conflicting type (uint32_t, size_t, etc.)
// 2. Note what base type the system uses (e.g., __uint32_t vs unsigned int)
// 3. Find your platform's int.h file from the list above
// 4. Modify the fl:: type to use the same base type as the system
// 5. Add version checks if needed (see ESP32 IDF 3.3 example in fl/int.h)
// 6. Apply to BOTH C++ (namespace fl) and C sections if present
// 7. Test compilation
//
// ============================================================================
// LEARN FROM PAST FIXES
// ============================================================================
//
// Research commands:
//   git log --oneline --all -- 'src/platforms/**/int*.h'
//   git show 32b3c80  # ESP32 IDF 3.3 fix
//   git show 4788f81  # Teensy 4.1 fix
//
// Detailed troubleshooting guide: see src/fl/int.h (top of file)
//
// ============================================================================
// KEY PRINCIPLE
// ============================================================================
//
// This file is a THIN WRAPPER that creates standard names for FastLED types.
// When conflicts occur, we don't change the wrapper - we change the underlying
// platform types to match the system. This maintains FastLED's fast compilation
// while ensuring perfect compatibility with system headers.
//
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
// FastLED Standard Integer Type Definitions
//
// This file provides both C and C++ support for compatibility with third-party
// code that includes fl/stdint.h directly (e.g., kiss_fft.h).
//
// For C code: Delegates to fl/cstdint.h
// For C++ code: Uses fl:: namespace types
///////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus

#include "fl/int.h"
#include "fl/stl/cstddef.h"

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
typedef fl::u64 uint64_t;
typedef fl::i64 int64_t;
typedef fl::size size_t;
typedef fl::uptr uintptr_t;
typedef fl::iptr intptr_t;
typedef fl::ptrdiff ptrdiff_t;

#else
// C language support: delegate to fl/cstdint.h
#include "fl/stl/cstdint.h"

#endif  // __cplusplus
