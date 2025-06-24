/// @file intmap.h
/// Defines integer mapping functions

#pragma once

#include "fl/namespace.h"
#include "lib8static.h"
#include "fl/stdint.h"

FASTLED_NAMESPACE_BEGIN

/// @addtogroup lib8tion
/// @{

/// @defgroup intmap Integer Mapping Functions
/// Maps a scalar from one integer size to another.
///
/// For example, a value representing 40% as an 8-bit unsigned integer would be
/// `102 / 255`. Using `map8_to_16(uint8_t)` to convert that to a 16-bit
/// unsigned integer would give you `26,214 / 65,535`, exactly 40% through the
/// larger range.
///
/// @{

LIB8STATIC_ALWAYS_INLINE uint16_t map8_to_16(uint8_t x) {
    return uint16_t(x) * 0x101;
}

LIB8STATIC_ALWAYS_INLINE uint32_t map16_to_32(uint16_t x) {
    return uint32_t(x) * 0x10001;
}

// map16_to_8: map 16-bit values to 8-bit values
//   This function maps 16-bit values to 8-bit values.
LIB8STATIC_ALWAYS_INLINE uint8_t map16_to_8(uint16_t x) {
    // Tested to be nearly identical to double precision floating point
    // doing this operation.
    if (x == 0) {
        return 0;
    }
    if (x >= 0xff00) {
        return 0xff;
    }
    return uint8_t((x + 128) >> 8);
}

LIB8STATIC_ALWAYS_INLINE uint16_t map32_to_16(uint32_t x) {
    // Tested to be nearly identical to double precision floating point
    // doing this operation.
    if (x == 0) {
        return 0;
    }
    if (x >= 0xffff0000) {
        return 0xffff;
    }
    return uint16_t((x + 32768) >> 16);
}

LIB8STATIC_ALWAYS_INLINE uint32_t map8_to_32(uint8_t x) {
    return uint32_t(x) * 0x1010101;
}

/// @} intmap
/// @} lib8tion

FASTLED_NAMESPACE_END
