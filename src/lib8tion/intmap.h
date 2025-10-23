/// @file intmap.h
/// @brief Integer mapping functions between different integer sizes.
///
/// This file re-exports the integer mapping functions from the platform-specific
/// implementation into the fl namespace for backward compatibility.

#pragma once

#include "platforms/intmap.h"

namespace fl {
/// @addtogroup lib8tion
/// @{

/// @defgroup intmap Integer Mapping Functions
/// Maps a scalar from one integer size to another.
/// @{

// Bring platform implementations into fl namespace
using fl::details::map8_to_16;
using fl::details::smap8_to_16;
using fl::details::map8_to_32;
using fl::details::smap8_to_32;
using fl::details::map16_to_32;
using fl::details::smap16_to_32;
using fl::details::map16_to_8;
using fl::details::smap16_to_8;
using fl::details::map32_to_16;
using fl::details::smap32_to_16;
using fl::details::map32_to_8;
using fl::details::smap32_to_8;

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
template <typename INT_FROM, typename INT_TO>
inline INT_TO int_scale(typename fl::identity<INT_FROM>::type x) {
    return details::int_scale<INT_FROM, INT_TO>(x);
}

/// @} intmap
/// @} lib8tion
}  // namespace fl
