#pragma once

///////////////////////////////////////////////////////////////////////
///
/// @defgroup FastMemory Fast Memory Functions for AVR
/// Alternatives to memmove, memcpy, and memset that are
/// faster on AVR than standard avr-libc 1.8. 
/// @{

#if defined(__AVR__) || defined(FASTLED_DOXYGEN)
extern "C" {
void * memmove8( void * dst, const void * src, uint16_t num );  ///< Faster alternative to memmove() on AVR
void * memcpy8 ( void * dst, const void * src, uint16_t num )  __attribute__ ((noinline));  ///< Faster alternative to memcpy() on AVR
void * memset8 ( void * ptr, uint8_t value, uint16_t num ) __attribute__ ((noinline)) ;  ///< Faster alternative to memset() on AVR
}
#else
#include "fl/memfill.h"
// on non-AVR platforms, these names just call standard libc.
#define memmove8 memmove
#define memcpy8 fl::memcopy
#define memset8 fl::memfill
#endif

/// @} FastMemory
