#pragma once

///////////////////////////////////////////////////////////////////////////////
// FastLED C Language Integer Type Definitions
//
// IMPORTANT: This file is for C language compilation ONLY. C++ code should
// use fl/stdint.h which provides the fl:: namespace types.
//
// FastLED has carefully purged <stdint.h> from the include path to avoid
// ~500ms compilation time overhead per object file. Instead, FastLED defines
// its own integer types using primitive types in platform-specific int.h files.
//
// This file provides standard C integer type names (uint8_t, int32_t, etc.)
// as typedefs of platform-specific types, plus stddef types (size_t, ptrdiff_t).
///////////////////////////////////////////////////////////////////////////////

// Include platform-specific type definitions (no namespace for C)
#include "platforms/int.h"  // IWYU pragma: keep

// Define standard integer type names for C code
// 8-bit types use raw primitives to match system headers exactly
typedef unsigned char uint8_t;
typedef signed char int8_t;

// 16-bit types use platform-specific base types
typedef fl::u16 uint16_t;
typedef fl::i16 int16_t;

// 32-bit types use platform-specific base types
typedef fl::u32 uint32_t;
typedef fl::i32 int32_t;

// 64-bit types use platform-specific base types
typedef fl::u64 uint64_t;
typedef fl::i64 int64_t;

// Pointer and size types for C code
// These are defined here directly for C language support (no C++ namespace needed)
#ifndef FL_SIZE_T_DEFINED
#define FL_SIZE_T_DEFINED
typedef __SIZE_TYPE__ size_t;
#endif

#ifndef FL_PTRDIFF_T_DEFINED
#define FL_PTRDIFF_T_DEFINED
#ifdef __PTRDIFF_TYPE__
typedef __PTRDIFF_TYPE__ ptrdiff_t;
#else
// Fallback for compilers that don't define __PTRDIFF_TYPE__ (like older AVR-GCC)
typedef long ptrdiff_t;
#endif
#endif

// uintptr_t and intptr_t from platform-specific types
typedef fl::uptr uintptr_t;
typedef fl::iptr intptr_t;

// stdint.h limit macros
// Include the platform's <stdint.h> to get standard limit macros like INT32_MAX, UINT64_MAX, etc.
// We include this at the END of the file after defining our types to avoid typedef conflicts.
// The macros don't conflict and we need them for bounds checking in various parts of the codebase.
#include <stdint.h>  // For INT8_MAX, UINT64_MAX, etc.
