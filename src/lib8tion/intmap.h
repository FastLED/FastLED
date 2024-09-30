
#include "namespace.h"
#include "lib8static.h"
#include <stdint.h>

FASTLED_NAMESPACE_BEGIN

LIB8STATIC_ALWAYS_INLINE uint16_t map8_to_16(uint8_t x) {
    return uint16_t(x) * 0x101;
}

LIB8STATIC_ALWAYS_INLINE uint32_t map8_to_32(uint8_t x) {
    return uint32_t(x) * 0x1010101;
}

LIB8STATIC_ALWAYS_INLINE uint16_t map16_to_32(uint16_t x) {
    return uint32_t(x) * 0x10001;
}

FASTLED_NAMESPACE_END
