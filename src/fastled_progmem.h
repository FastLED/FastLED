#pragma once

#include "platforms/is_platform.h"  // ok platform headers
#include "fl/int.h"  // ok platform headers

#if defined(__EMSCRIPTEN__) || defined(FASTLED_TESTING) || defined(FASTLED_STUB_IMPL)
#include "platforms/null_progmem.h"  // ok platform headers
#elif defined(ESP8266)
#include "platforms/esp/8266/progmem_esp8266.h"  // ok platform headers
#elif defined(FL_IS_STM32)
#include "platforms/arm/stm32/progmem_stm32.h"  // ok platform headers
#else




/// @file fastled_progmem.h
/// Wrapper definitions to allow seamless use of PROGMEM in environments that have it
///
/// This is a compatibility layer for devices that do or don't
/// have "PROGMEM" and the associated pgm_ accessors.
///
/// If a platform supports PROGMEM, it should define
/// `FASTLED_USE_PROGMEM` as 1, otherwise FastLED will
/// fall back to NOT using PROGMEM.
///
/// Whether or not pgmspace.h is \#included is separately
/// controllable by FASTLED_INCLUDE_PGMSPACE, if needed.



// This block is used for Doxygen documentation generation,
// so that the Doxygen parser will be able to find the macros
// included without a defined platform
#ifdef FASTLED_DOXYGEN
#define FASTLED_USE_PROGMEM 1
#endif

/// @def FASTLED_USE_PROGMEM
/// Determine whether the current platform supports PROGMEM. 
/// If FASTLED_USE_PROGMEM is 1, we'll map FL_PROGMEM
/// and the FL_PGM_* accessors to the Arduino equivalents.


#if (FASTLED_USE_PROGMEM == 1) || defined(FASTLED_DOXYGEN)
#ifndef FASTLED_INCLUDE_PGMSPACE
#define FASTLED_INCLUDE_PGMSPACE 1
#endif

#if FASTLED_INCLUDE_PGMSPACE == 1
// IWYU pragma: begin_keep
#include <avr/pgmspace.h>
// IWYU pragma: end_keep
#endif

/// PROGMEM keyword for storage
#define FL_PROGMEM                PROGMEM


/// @name PROGMEM Read Functions
/// Functions for reading data from PROGMEM memory.
///
/// Note that only the "near" memory wrappers are provided.
/// If you're using "far" memory, you already have
/// portability issues to work through, but you could
/// add more support here if needed.
///
/// @{

/// Read a byte (8-bit) from PROGMEM memory
#define FL_PGM_READ_BYTE_NEAR(x)  (pgm_read_byte_near(x))
/// Read a word (16-bit) from PROGMEM memory
#define FL_PGM_READ_WORD_NEAR(x)  (pgm_read_word_near(x))
/// Read a double word (32-bit) from PROGMEM memory
#define FL_PGM_READ_DWORD_NEAR(x) (pgm_read_dword_near(x))

/// @} PROGMEM

// Aligned 4-byte PROGMEM read using AVR pgm_read_dword_near.
#define FL_PGM_READ_DWORD_ALIGNED(addr) ((fl::u32)pgm_read_dword_near(addr))

// Workaround for http://gcc.gnu.org/bugzilla/show_bug.cgi?id=34734
#if __GNUC__ < 4 || (__GNUC__ == 4 && (__GNUC_MINOR__ < 6))
#ifdef FASTLED_AVR
#ifdef PROGMEM
#undef PROGMEM
#define PROGMEM __attribute__((section(".progmem.data")))
#endif
#endif
#endif

#else
// If FASTLED_USE_PROGMEM is 0 or undefined,
// we'll use regular memory (RAM) access.

#include "platforms/null_progmem.h"

#endif

/// @def FL_ALIGN_PROGMEM(N)
/// Force N-byte alignment for platforms with unaligned access or cache-line optimization
///
/// On some platforms, most notably ARM M0, unaligned access
/// to 'PROGMEM' for multibyte values (e.g. read dword) is
/// not allowed and causes a crash.  This macro can help
/// force alignment as needed.  The FastLED gradient
/// palette code uses 'read dword', and now uses this macro
/// to make sure that gradient palettes are 4-byte aligned.
///
/// Usage: FL_ALIGN_PROGMEM(N) where N is the alignment in bytes
/// - FL_ALIGN_PROGMEM(4):  4-byte alignment (safe default for all platforms)
/// - FL_ALIGN_PROGMEM(64): Cache-line optimization on x86/ARM/ESP
///
/// Platform-specific behavior:
/// - x86/WASM: Uses __attribute__((aligned(N))) for cache-line optimization
/// - AVR: Caps at 4 bytes (flash PROGMEM doesn't benefit from larger alignment)
/// - ESP32/ESP8266: Supports full alignment (cache-line optimization)
/// - ARM: Supports full alignment

#ifndef FL_ALIGN_PROGMEM
#if defined(FL_IS_ARM) || defined(ESP32) || defined(FASTLED_DOXYGEN)
#define FL_ALIGN_PROGMEM(N)  __attribute__ ((aligned (N)))
#else
#define FL_ALIGN_PROGMEM(N)
#endif
#endif

#endif  // platform progmem selection
