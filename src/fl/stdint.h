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


// Define standard integer type names as aliases to FastLED types in global namespace
// This avoids the slow <stdint.h> include while maintaining compatibility
// The compile tests in platforms/compile_test.cpp enforce that fl::u8 == ::uint8_t, etc.
typedef fl::u8 uint8_t;
typedef fl::i8 int8_t;
typedef fl::u16 uint16_t;
typedef fl::i16 int16_t;
typedef fl::u32 uint32_t;
typedef fl::i32 int32_t;
typedef fl::u64 uint64_t;
typedef fl::i64 int64_t;
typedef fl::size size_t;
typedef fl::uptr uintptr_t;
typedef fl::ptrdiff_t ptrdiff_t;

#ifndef INT16_MIN
#define INT16_MIN   (-32768)
#endif
#ifndef INT16_MAX
#define INT16_MAX   32767
#endif
#ifndef UINT16_MAX
#define UINT16_MAX  65535U
#endif

#ifndef INT32_MIN
#define INT32_MIN   (-2147483647 - 1)
#endif
#ifndef INT32_MAX
#define INT32_MAX   2147483647
#endif
#ifndef UINT32_MAX
#define UINT32_MAX  4294967295U
#endif

#ifndef INT64_MIN
#define INT64_MIN   (-9223372036854775807LL - 1)
#endif
#ifndef INT64_MAX
#define INT64_MAX   9223372036854775807LL
#endif
#ifndef UINT64_MAX
#define UINT64_MAX  18446744073709551615ULL
#endif