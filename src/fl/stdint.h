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


// Define standard integer type names using raw primitive types
// This avoids the slow <stdint.h> include while maintaining compatibility
// IMPORTANT: Use raw primitive types (not fl:: typedefs) to match system headers exactly
//            This allows duplicate typedefs when system headers are also included
typedef unsigned char uint8_t;
typedef signed char int8_t;
typedef unsigned short uint16_t;
typedef short int16_t;

// Define standard types using fl:: types from platform-specific int.h
// This ensures we match the platform's type sizes correctly
typedef fl::u32 uint32_t;
typedef fl::i32 int32_t;
typedef fl::size size_t;
typedef fl::uptr uintptr_t;
typedef fl::ptrdiff ptrdiff_t;

typedef unsigned long long uint64_t;
typedef long long int64_t;

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
