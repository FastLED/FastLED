/// @file intmap.h
/// @brief Integer mapping functions between different integer sizes.
///
/// This file provides the generic int_scale<FROM, TO>() template function
/// and deprecated legacy named functions that delegate to it for backward compatibility.

#pragma once

#include "platforms/intmap.h"
#include "fl/stl/math.h"

namespace fl {
/// @addtogroup lib8tion
/// @{

/// @defgroup intmap Integer Mapping Functions
/// Maps a scalar from one integer size to another.
/// @{

/// @brief Generic integer scaling between different integer sizes.
///
/// @tparam INT_FROM Input integer type (must be explicitly specified)
/// @tparam INT_TO Output integer type (uint8_t, int8_t, uint16_t, int16_t, uint32_t, int32_t)
/// @param x The input value to scale
/// @return The scaled value preserving relative position in range
///
/// @details
/// Requires explicit specification of BOTH input and output types to prevent
/// implicit type conversions that could mask bugs. Uses bit replication for
/// scaling up (8→16, 8→32, 16→32) and right-shift with rounding for
/// scaling down (16→8, 32→16, 32→8).
///
/// Supports all combinations of 8-bit, 16-bit, and 32-bit signed/unsigned types.
/// Compile-time error for unsupported type conversions.
///
/// @example
/// ```cpp
/// uint16_t y = fl::int_scale<uint8_t, uint16_t>(102);   // Explicit: 8→16
/// int32_t z = fl::int_scale<int8_t, int32_t>(64);       // Explicit: 8→32
/// uint8_t w = fl::int_scale<uint32_t, uint8_t>(0x80000000);  // Explicit: 32→8
/// uint8_t noop = fl::int_scale<uint8_t, uint8_t>(255);   // Identity: no scaling
/// ```

// Deprecated legacy unsigned functions that delegate to the new int_scale<FROM, TO>() template
/// @deprecated Use int_scale<uint8_t, uint16_t>() instead

inline uint16_t map8_to_16(uint8_t x) {
    return fl::int_scale<uint8_t, uint16_t>(x);
}

/// @deprecated Use int_scale<uint8_t, uint32_t>() instead

inline uint32_t map8_to_32(uint8_t x) {
    return fl::int_scale<uint8_t, uint32_t>(x);
}

/// @deprecated Use int_scale<uint16_t, uint32_t>() instead

inline uint32_t map16_to_32(uint16_t x) {
    return fl::int_scale<uint16_t, uint32_t>(x);
}

/// @deprecated Use int_scale<uint16_t, uint8_t>() instead

inline uint8_t map16_to_8(uint16_t x) {
    return fl::int_scale<uint16_t, uint8_t>(x);
}

/// @deprecated Use int_scale<uint32_t, uint16_t>() instead

inline uint16_t map32_to_16(uint32_t x) {
    return fl::int_scale<uint32_t, uint16_t>(x);
}

/// @deprecated Use int_scale<uint32_t, uint8_t>() instead
inline uint8_t map32_to_8(uint32_t x) {
    return fl::int_scale<uint32_t, uint8_t>(x);
}

// New signed functions that delegate to int_scale
/// Use int_scale<int8_t, int16_t>() instead
inline int16_t smap8_to_16(int8_t x) {
    return fl::int_scale<int8_t, int16_t>(x);
}

/// Use int_scale<int8_t, int32_t>() instead
inline int32_t smap8_to_32(int8_t x) {
    return fl::int_scale<int8_t, int32_t>(x);
}

/// Use int_scale<int16_t, int32_t>() instead
inline int32_t smap16_to_32(int16_t x) {
    return fl::int_scale<int16_t, int32_t>(x);
}

/// Use int_scale<int16_t, int8_t>() instead
inline int8_t smap16_to_8(int16_t x) {
    return fl::int_scale<int16_t, int8_t>(x);
}

/// Use int_scale<int32_t, int16_t>() instead
inline int16_t smap32_to_16(int32_t x) {
    return fl::int_scale<int32_t, int16_t>(x);
}

/// Use int_scale<int32_t, int8_t>() instead
inline int8_t smap32_to_8(int32_t x) {
    return fl::int_scale<int32_t, int8_t>(x);
}

/// @} intmap
/// @} lib8tion
}  // namespace fl
