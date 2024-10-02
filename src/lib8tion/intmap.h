
#pragma once

#include "namespace.h"
#include "lib8static.h"
#include <stdint.h>

FASTLED_NAMESPACE_BEGIN

LIB8STATIC_ALWAYS_INLINE uint16_t map8_to_16(uint8_t x) {
    return uint16_t(x) * 0x101;
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

LIB8STATIC_ALWAYS_INLINE uint32_t map8_to_32(uint8_t x) {
    return uint32_t(x) * 0x1010101;
}

FASTLED_NAMESPACE_END
