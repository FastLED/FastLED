#pragma once

// Include fl/int.h to get FastLED integer types (u8, u16, i8, etc.)
// This defines types using primitive types which compiles faster than <stdint.h>
#include "fl/int.h"

#ifdef __cplusplus
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
#else
// C language compatibility - types are already defined in global namespace by fl/int.h
// Include standard headers to ensure compatibility
#include <stdint.h>
#include <stddef.h>
#endif
