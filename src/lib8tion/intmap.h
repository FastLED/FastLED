/// @file intmap.h
/// @brief Integer mapping functions between different integer sizes.
///
/// @details
/// Maps scalar values between different integer sizes while preserving their
/// relative position within their respective ranges (e.g., 50% of 8-bit range
/// becomes 50% of 16-bit range).
///
/// @section intmap_scaling_up Scaling Up (8→16, 8→32, 16→32)
/// Uses bit replication via multiplication. For example, an 8-bit value 0xAB
/// becomes 0xABAB when scaled to 16-bit (multiply by 0x0101). This ensures
/// that the maximum value (0xFF → 0xFFFF) and minimum (0x00 → 0x0000) map
/// correctly without floating point operations.
///
/// @section intmap_scaling_down Scaling Down (16→8, 32→16)
/// Uses right-shift with rounding to convert back to smaller sizes. The rounding
/// constant (128 for 8-bit, 32768 for 16-bit) is added before shifting to achieve
/// proper nearest-neighbor rounding rather than truncation.

#pragma once

#include "lib8static.h"
#include "fl/stdint.h"


namespace fl {
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

/// @brief Maps an 8-bit value to a 16-bit value.
/// @param x The 8-bit input value to scale up.
/// @return The scaled 16-bit value preserving the relative position in the range.
///
/// @details
/// Scales to the larger range using bit replication via multiplication by 0x0101 (257).
/// The byte is replicated: 0xAB → 0xABAB, which is equivalent to @c (x << 8) | x.
/// Correctly maps both endpoints: 0x00 → 0x0000 and 0xFF → 0xFFFF.
///
/// @note Performs no floating point operations; suitable for embedded systems.
LIB8STATIC_ALWAYS_INLINE uint16_t map8_to_16(uint8_t x) {
    return uint16_t(x) * 0x101;
}

/// @brief Maps an 8-bit value to a 32-bit value.
/// @param x The 8-bit input value to scale up.
/// @return The scaled 32-bit value preserving the relative position in the range.
///
/// @details
/// Scales to the larger range using bit replication via multiplication by 0x01010101.
/// The byte is replicated across all 4 bytes: 0xAB → 0xABABABAB.
/// Correctly maps both endpoints: 0x00 → 0x00000000 and 0xFF → 0xFFFFFFFF.
///
/// @note Performs no floating point operations; suitable for embedded systems.
LIB8STATIC_ALWAYS_INLINE uint32_t map8_to_32(uint8_t x) {
    return uint32_t(x) * 0x1010101;
}

/// @brief Maps a 16-bit value to a 32-bit value.
/// @param x The 16-bit input value to scale up.
/// @return The scaled 32-bit value preserving the relative position in the range.
///
/// @details
/// Scales to the larger range using bit replication via multiplication by 0x00010001 (65537).
/// The word is replicated: 0xABCD → 0xABCDABCD.
/// Correctly maps both endpoints: 0x0000 → 0x00000000 and 0xFFFF → 0xFFFFFFFF.
///
/// @note Performs no floating point operations; suitable for embedded systems.
LIB8STATIC_ALWAYS_INLINE uint32_t map16_to_32(uint16_t x) {
    return uint32_t(x) * 0x10001;
}


/// @brief Maps a 16-bit value down to an 8-bit value.
/// @param x The 16-bit input value to scale down.
/// @return The scaled 8-bit value preserving the relative position in the range.
///
/// @details
/// Scales to the smaller range using right-shift with rounding by 8 bits.
/// The rounding constant 128 (0x80) is added before shifting to implement
/// nearest-neighbor rounding rather than truncation. This ensures 0x7F80
/// rounds up to 0x80 instead of truncating down to 0x7F.
/// Special cases handle boundary values (0 and 0xFF) to prevent overflow.
///
/// @note Tested to produce results nearly identical to double-precision floating-point
///       division while remaining integer-only.
LIB8STATIC_ALWAYS_INLINE uint8_t map16_to_8(uint16_t x) {
    if (x == 0) {
        return 0;
    }
    if (x >= 0xff00) {
        return 0xff;
    }
    return uint8_t((x + 128) >> 8);
}

/// @brief Maps a 32-bit value down to a 16-bit value.
/// @param x The 32-bit input value to scale down.
/// @return The scaled 16-bit value preserving the relative position in the range.
///
/// @details
/// Scales to the smaller range using right-shift with rounding by 16 bits.
/// The rounding constant 32768 (0x8000) is added before shifting to implement
/// nearest-neighbor rounding, ensuring proper rounding of midpoint values
/// during the downscaling operation.
/// Special cases handle boundary values (0 and 0xFFFF) to prevent overflow.
///
/// @note Tested to produce results nearly identical to double-precision floating-point
///       division while remaining integer-only.
LIB8STATIC_ALWAYS_INLINE uint16_t map32_to_16(uint32_t x) {
    if (x == 0) {
        return 0;
    }
    if (x >= 0xffff0000) {
        return 0xffff;
    }
    return uint16_t((x + 32768) >> 16);
}

/// @brief Maps a 32-bit value down to an 8-bit value.
/// @param x The 32-bit input value to scale down.
/// @return The scaled 8-bit value preserving the relative position in the range.
///
/// @details
/// Scales to the smaller range using right-shift with rounding by 24 bits.
/// The rounding constant 8388608 (0x800000) is added before shifting to implement
/// nearest-neighbor rounding, ensuring proper rounding of midpoint values
/// during the downscaling operation from 32-bit to 8-bit.
/// Special cases handle boundary values (0 and 0xFF) to prevent overflow.
///
/// @note Tested to produce results nearly identical to double-precision floating-point
///       division while remaining integer-only.
LIB8STATIC_ALWAYS_INLINE uint8_t map32_to_8(uint32_t x) {
    if (x == 0) {
        return 0;
    }
    if (x >= 0xffffff00) {
        return 0xff;
    }
    return uint8_t((x + 0x800000) >> 24);
}

/// @} intmap
/// @} lib8tion
}  // namespace fl
