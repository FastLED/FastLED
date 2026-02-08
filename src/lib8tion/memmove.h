#pragma once

#include "platforms/is_platform.h"

///////////////////////////////////////////////////////////////////////
///
/// @defgroup FastMemory Fast Memory Functions for AVR
/// Alternatives to memmove, memcpy, and memset that are
/// faster on AVR than standard avr-libc 1.8.
/// @{

#if defined(FL_IS_AVR) || defined(FASTLED_DOXYGEN)
extern "C" {
void * memmove8( void * dst, const void * src, fl::u16 num );  ///< Faster alternative to memmove() on AVR
void * memcpy8 ( void * dst, const void * src, fl::u16 num )  __attribute__ ((noinline));  ///< Faster alternative to memcpy() on AVR
void * memset8 ( void * ptr, fl::u8 value, fl::u16 num ) __attribute__ ((noinline)) ;  ///< Faster alternative to memset() on AVR
}
#else
#include "fl/stl/cstring.h"
// on non-AVR platforms, these names just call standard libc.
#define memmove8 memmove
#define memcpy8 fl::memcpy
#define memset8 fl::memset
#endif

/// @} FastMemory
