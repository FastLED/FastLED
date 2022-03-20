#ifndef __INC_FL_PROGMEM_H
#define __INC_FL_PROGMEM_H

#include "FastLED.h"

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

FASTLED_NAMESPACE_BEGIN


// This block is used for Doxygen documentation generation,
// so that the Doxygen parser will be able to find the macros
// included without a defined platform
#ifdef FASTLED_DOXYGEN
#define FASTLED_USE_PROGMEM 1
#define FL_ALIGN_PROGMEM  __attribute__ ((aligned (4)))
#endif

/// @def FASTLED_USE_PROGMEM
/// Determine whether the current platform supports PROGMEM. 
/// If FASTLED_USE_PROGMEM is 1, we'll map FL_PROGMEM
/// and the FL_PGM_* accessors to the Arduino equivalents.


#if FASTLED_USE_PROGMEM == 1
#ifndef FASTLED_INCLUDE_PGMSPACE
#define FASTLED_INCLUDE_PGMSPACE 1
#endif

#if FASTLED_INCLUDE_PGMSPACE == 1
#include <avr/pgmspace.h>
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

//empty PROGMEM simulation
#define FL_PROGMEM
#define FL_PGM_READ_BYTE_NEAR(x)  (*((const  uint8_t*)(x)))
#define FL_PGM_READ_WORD_NEAR(x)  (*((const uint16_t*)(x)))
#define FL_PGM_READ_DWORD_NEAR(x) (*((const uint32_t*)(x)))

#endif

/// @def FL_ALIGN_PROGMEM
/// Helps to force 4-byte alignment for platforms with unaligned access
///
/// On some platforms, most notably ARM M0, unaligned access
/// to 'PROGMEM' for multibyte values (e.g. read dword) is
/// not allowed and causes a crash.  This macro can help
/// force 4-byte alignment as needed.  The FastLED gradient
/// palette code uses 'read dword', and now uses this macro
/// to make sure that gradient palettes are 4-byte aligned.

#if defined(FASTLED_ARM) || defined(ESP32) || defined(ESP8266)
#define FL_ALIGN_PROGMEM  __attribute__ ((aligned (4)))
#else
#define FL_ALIGN_PROGMEM
#endif


FASTLED_NAMESPACE_END

#endif
