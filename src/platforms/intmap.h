/// @file intmap.h
/// @brief Platform-specific integer mapping and scaling functions.
///
/// @details
/// Maps scalar values between different integer sizes while preserving their
/// relative position within their respective ranges (e.g., 50% of 8-bit range
/// becomes 50% of 16-bit range).
///
/// Provides both legacy named functions (map8_to_16, map16_to_8, etc.) and
/// a modern generic `int_scale<INT_TO, INT_FROM>()` template that handles all combinations.
///
/// @section intmap_scaling_up Scaling Up (8→16, 8→32, 16→32)
/// Uses bit replication via multiplication. For example, an 8-bit value 0xAB
/// becomes 0xABAB when scaled to 16-bit (multiply by 0x0101). This ensures
/// that the maximum value (0xFF → 0xFFFF) and minimum (0x00 → 0x0000) map
/// correctly without floating point operations.
///
/// @section intmap_scaling_down Scaling Down (16→8, 32→16, 32→8)
/// Uses right-shift with rounding to convert back to smaller sizes. The rounding
/// constant (128 for 8-bit, 32768 for 16-bit) is added before shifting to achieve
/// proper nearest-neighbor rounding rather than truncation.

#pragma once

#include "lib8tion/lib8static.h"
#include "fl/stl/stdint.h"
#include "fl/sketch_macros.h"
#include "fl/stl/type_traits.h"
#include "fl/compiler_control.h"
#include "fl/force_inline.h"

namespace fl {
namespace details {

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

// Forward declarations for legacy functions (defined below)
FL_ALWAYS_INLINE uint16_t map8_to_16(uint8_t x);
FL_ALWAYS_INLINE int16_t smap8_to_16(int8_t x);
FL_ALWAYS_INLINE uint32_t map8_to_32(uint8_t x);
FL_ALWAYS_INLINE int32_t smap8_to_32(int8_t x);
FL_ALWAYS_INLINE uint32_t map16_to_32(uint16_t x);
FL_ALWAYS_INLINE int32_t smap16_to_32(int16_t x);
FL_ALWAYS_INLINE uint8_t map16_to_8(uint16_t x);
FL_ALWAYS_INLINE int8_t smap16_to_8(int16_t x);
FL_ALWAYS_INLINE uint16_t map32_to_16(uint32_t x);
FL_ALWAYS_INLINE int16_t smap32_to_16(int32_t x);
FL_ALWAYS_INLINE uint8_t map32_to_8(uint32_t x);
FL_ALWAYS_INLINE int8_t smap32_to_8(int32_t x);

// ============================================================================
// int_scale_impl: C++11-compatible template specializations
// ============================================================================

/// @brief Primary template for int_scale conversion (defaults to static_assert for invalid pairs)
template <typename INT_FROM, typename INT_TO>
struct int_scale_impl {
    // Provide static_assert error for unsupported conversions
    // This gets overridden by specializations for valid type pairs
    static INT_TO apply(INT_FROM) {
        // This will only be instantiated if no specialization matches
        // Use dependent expression to delay assertion until instantiation
        static_assert(fl::is_same<INT_FROM, void>::value, "int_scale: unsupported type conversion pair");
        return INT_TO(0);  // Never reached, but satisfies return type
    }
};

// ============================================================================
// Scaling Up (8→16, 8→32, 16→32)
// ============================================================================

// uint8_t → uint16_t
template <>
struct int_scale_impl<uint8_t, uint16_t> {
    FL_ALWAYS_INLINE uint16_t apply(uint8_t x) {
        return uint16_t(x) * 0x101;
    }
};

// int8_t → int16_t
template <>
struct int_scale_impl<int8_t, int16_t> {
    FL_ALWAYS_INLINE int16_t apply(int8_t x) {
        return int16_t(uint16_t(uint8_t(x)) * 0x101);
    }
};

// uint8_t → int16_t
template <>
struct int_scale_impl<uint8_t, int16_t> {
    FL_ALWAYS_INLINE int16_t apply(uint8_t x) {
        return int16_t(uint16_t(x) * 0x101);
    }
};

// int8_t → uint16_t
template <>
struct int_scale_impl<int8_t, uint16_t> {
    FL_ALWAYS_INLINE uint16_t apply(int8_t x) {
        return uint16_t(uint8_t(x)) * 0x101;
    }
};

// uint8_t → uint32_t
template <>
struct int_scale_impl<uint8_t, uint32_t> {
    FL_ALWAYS_INLINE uint32_t apply(uint8_t x) {
        return uint32_t(x) * 0x1010101;
    }
};

// int8_t → int32_t
template <>
struct int_scale_impl<int8_t, int32_t> {
    FL_ALWAYS_INLINE int32_t apply(int8_t x) {
        return int32_t(uint32_t(uint8_t(x)) * 0x1010101);
    }
};

// uint8_t → int32_t
template <>
struct int_scale_impl<uint8_t, int32_t> {
    FL_ALWAYS_INLINE int32_t apply(uint8_t x) {
        return int32_t(uint32_t(x) * 0x1010101);
    }
};

// int8_t → uint32_t
template <>
struct int_scale_impl<int8_t, uint32_t> {
    FL_ALWAYS_INLINE uint32_t apply(int8_t x) {
        return uint32_t(uint8_t(x)) * 0x1010101;
    }
};

// uint16_t → uint32_t
template <>
struct int_scale_impl<uint16_t, uint32_t> {
    FL_ALWAYS_INLINE uint32_t apply(uint16_t x) {
        return uint32_t(x) * 0x10001;
    }
};

// int16_t → int32_t
template <>
struct int_scale_impl<int16_t, int32_t> {
    FL_ALWAYS_INLINE int32_t apply(int16_t x) {
        return int32_t(uint32_t(uint16_t(x)) * 0x10001);
    }
};

// uint16_t → int32_t
template <>
struct int_scale_impl<uint16_t, int32_t> {
    FL_ALWAYS_INLINE int32_t apply(uint16_t x) {
        return int32_t(uint32_t(x) * 0x10001);
    }
};

// int16_t → uint32_t
template <>
struct int_scale_impl<int16_t, uint32_t> {
    FL_ALWAYS_INLINE uint32_t apply(int16_t x) {
        return uint32_t(uint16_t(x)) * 0x10001;
    }
};

// ============================================================================
// Scaling Down (16→8, 32→16, 32→8)
// ============================================================================

// uint16_t → uint8_t
template <>
struct int_scale_impl<uint16_t, uint8_t> {
    FL_ALWAYS_INLINE uint8_t apply(uint16_t x) {
#if SKETCH_HAS_LOTS_OF_MEMORY == 0
        if (x >= 0xff00) return 0xff;
        return uint8_t((x + 128) >> 8);
#else
        uint8_t result = (x + 128) >> 8;
        return (x >= 0xff00) ? 0xff : result;
#endif
    }
};

// int16_t → int8_t
template <>
struct int_scale_impl<int16_t, int8_t> {
    FL_ALWAYS_INLINE int8_t apply(int16_t x) {
#if SKETCH_HAS_LOTS_OF_MEMORY == 0
        if (x >= 0x7f80) return 127;
        return int8_t((x + 128) >> 8);
#else
        uint8_t result = (uint16_t(x) + 128) >> 8;
        return (x >= 0x7f80) ? 127 : int8_t(result);
#endif
    }
};

// uint16_t → int8_t
template <>
struct int_scale_impl<uint16_t, int8_t> {
    FL_ALWAYS_INLINE int8_t apply(uint16_t x) {
#if SKETCH_HAS_LOTS_OF_MEMORY == 0
        if (x >= 0xff00) return 0xff;
        return int8_t((x + 128) >> 8);
#else
        uint8_t result = (x + 128) >> 8;
        return int8_t((x >= 0xff00) ? 0xff : result);
#endif
    }
};

// int16_t → uint8_t
template <>
struct int_scale_impl<int16_t, uint8_t> {
    FL_ALWAYS_INLINE uint8_t apply(int16_t x) {
#if SKETCH_HAS_LOTS_OF_MEMORY == 0
        if (x >= 0x7f80) return 255;
        return uint8_t((x + 128) >> 8);
#else
        uint8_t result = (uint16_t(x) + 128) >> 8;
        return (x >= 0x7f80) ? 255 : result;
#endif
    }
};

// uint32_t → uint16_t
template <>
struct int_scale_impl<uint32_t, uint16_t> {
    FL_ALWAYS_INLINE uint16_t apply(uint32_t x) {
#if SKETCH_HAS_LOTS_OF_MEMORY == 0
        if (x >= 0xffff0000) return 0xffff;
        return uint16_t((x + 32768) >> 16);
#else
        uint16_t result = (x + 32768) >> 16;
        return (x >= 0xffff0000) ? 0xffff : result;
#endif
    }
};

// int32_t → int16_t
template <>
struct int_scale_impl<int32_t, int16_t> {
    FL_ALWAYS_INLINE int16_t apply(int32_t x) {
#if SKETCH_HAS_LOTS_OF_MEMORY == 0
        if (x >= 0x7fff8000) return 32767;
        return int16_t((x + 32768) >> 16);
#else
        uint16_t result = (uint32_t(x) + 32768) >> 16;
        return (x >= 0x7fff8000) ? 32767 : int16_t(result);
#endif
    }
};

// uint32_t → int16_t
template <>
struct int_scale_impl<uint32_t, int16_t> {
    FL_ALWAYS_INLINE int16_t apply(uint32_t x) {
#if SKETCH_HAS_LOTS_OF_MEMORY == 0
        if (x >= 0xffff0000) return 0xffff;
        return int16_t((x + 32768) >> 16);
#else
        uint16_t result = (x + 32768) >> 16;
        return int16_t((x >= 0xffff0000) ? 0xffff : result);
#endif
    }
};

// int32_t → uint16_t
template <>
struct int_scale_impl<int32_t, uint16_t> {
    FL_ALWAYS_INLINE uint16_t apply(int32_t x) {
#if SKETCH_HAS_LOTS_OF_MEMORY == 0
        if (x >= 0x7fff8000) return 0xffff;
        return uint16_t((x + 32768) >> 16);
#else
        uint16_t result = (uint32_t(x) + 32768) >> 16;
        return (x >= 0x7fff8000) ? 0xffff : result;
#endif
    }
};

// uint32_t → uint8_t
template <>
struct int_scale_impl<uint32_t, uint8_t> {
    FL_ALWAYS_INLINE uint8_t apply(uint32_t x) {
#if SKETCH_HAS_LOTS_OF_MEMORY == 0
        if (x >= 0xFF000000) return 0xff;
        return uint8_t((x + 0x800000) >> 24);
#else
        uint8_t result = (x + 0x800000) >> 24;
        return (x >= 0xFF000000) ? 0xff : result;
#endif
    }
};

// int32_t → int8_t
template <>
struct int_scale_impl<int32_t, int8_t> {
    FL_ALWAYS_INLINE int8_t apply(int32_t x) {
#if SKETCH_HAS_LOTS_OF_MEMORY == 0
        if (x >= 0x7F000000) return 127;
        return int8_t((x + 0x800000) >> 24);
#else
        uint8_t result = (uint32_t(x) + 0x800000) >> 24;
        return (x >= 0x7F000000) ? 127 : int8_t(result);
#endif
    }
};

// uint32_t → int8_t
template <>
struct int_scale_impl<uint32_t, int8_t> {
    FL_ALWAYS_INLINE int8_t apply(uint32_t x) {
#if SKETCH_HAS_LOTS_OF_MEMORY == 0
        if (x >= 0xFF000000) return 0x7f;
        return int8_t((x + 0x800000) >> 24);
#else
        uint8_t result = (x + 0x800000) >> 24;
        return int8_t((x >= 0xFF000000) ? 0x7f : result);
#endif
    }
};

// int32_t → uint8_t
template <>
struct int_scale_impl<int32_t, uint8_t> {
    FL_ALWAYS_INLINE uint8_t apply(int32_t x) {
#if SKETCH_HAS_LOTS_OF_MEMORY == 0
        if (x >= 0x7F000000) return 0xff;
        return uint8_t((x + 0x800000) >> 24);
#else
        uint8_t result = (uint32_t(x) + 0x800000) >> 24;
        return (x >= 0x7F000000) ? 0xff : result;
#endif
    }
};

// ============================================================================
// Identity specialization: same type in and out is a no-op
// ============================================================================

template <typename T>
struct int_scale_impl<T, T> {
    FL_ALWAYS_INLINE T apply(T x) {
        return x;
    }
};

// ============================================================================
// Generic int_scale template (dispatcher to int_scale_impl)
// ============================================================================

/// @brief Generic scaling dispatcher for different input/output types.
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
FL_ALWAYS_INLINE INT_TO int_scale(typename fl::identity<INT_FROM>::type x) {
    return int_scale_impl<INT_FROM, INT_TO>::apply(x);
}

// ============================================================================
// Legacy Named Mapping Functions
// ============================================================================

/// @brief Maps an 8-bit unsigned value to a 16-bit unsigned value.
/// @param x The 8-bit input value to scale up.
/// @return The scaled 16-bit value preserving the relative position in the range.
///
/// @details
/// Scales to the larger range using bit replication via multiplication by 0x0101 (257).
/// The byte is replicated: 0xAB → 0xABAB, which is equivalent to @c (x << 8) | x.
/// Correctly maps both endpoints: 0x00 → 0x0000 and 0xFF → 0xFFFF.
///
/// @note Performs no floating point operations; suitable for embedded systems.
FL_ALWAYS_INLINE uint16_t map8_to_16(uint8_t x) {
    return uint16_t(x) * 0x101;
}

/// @brief Maps an 8-bit signed value to a 16-bit signed value.
/// @param x The 8-bit signed input value to scale up.
/// @return The scaled 16-bit signed value preserving the relative position in the range.
///
/// @details
/// Scales to the larger range by casting through the unsigned equivalent and then
/// reinterpreting as signed. The bit pattern of x is replicated to fill the larger range.
/// Maps correctly: 0 → 0, 127 → 32639, -1 → -1, -128 → -32640.
///
/// @note Performs no floating point operations; suitable for embedded systems.
FL_ALWAYS_INLINE int16_t smap8_to_16(int8_t x) {
    // Cast signed to unsigned to preserve bit pattern, map with unsigned function,
    // then reinterpret result as signed
    return int16_t(map8_to_16(uint8_t(x)));
}

/// @brief Maps an 8-bit unsigned value to a 32-bit unsigned value.
/// @param x The 8-bit input value to scale up.
/// @return The scaled 32-bit value preserving the relative position in the range.
///
/// @details
/// Scales to the larger range using bit replication via multiplication by 0x01010101.
/// The byte is replicated across all 4 bytes: 0xAB → 0xABABABAB.
/// Correctly maps both endpoints: 0x00 → 0x00000000 and 0xFF → 0xFFFFFFFF.
///
/// @note Performs no floating point operations; suitable for embedded systems.
FL_ALWAYS_INLINE uint32_t map8_to_32(uint8_t x) {
    return uint32_t(x) * 0x1010101;
}

/// @brief Maps an 8-bit signed value to a 32-bit signed value.
/// @param x The 8-bit signed input value to scale up.
/// @return The scaled 32-bit signed value preserving the relative position in the range.
///
/// @details
/// Scales to the larger range by casting through the unsigned equivalent and then
/// reinterpreting as signed. The bit pattern of x is replicated to fill the larger range.
/// Maps correctly: 0 → 0, 127 → 2139062143, -1 → -1, -128 → -2139062144.
///
/// @note Performs no floating point operations; suitable for embedded systems.
FL_ALWAYS_INLINE int32_t smap8_to_32(int8_t x) {
    return int32_t(map8_to_32(uint8_t(x)));
}

/// @brief Maps a 16-bit unsigned value to a 32-bit unsigned value.
/// @param x The 16-bit input value to scale up.
/// @return The scaled 32-bit value preserving the relative position in the range.
///
/// @details
/// Scales to the larger range using bit replication via multiplication by 0x00010001 (65537).
/// The word is replicated: 0xABCD → 0xABCDABCD.
/// Correctly maps both endpoints: 0x0000 → 0x00000000 and 0xFFFF → 0xFFFFFFFF.
///
/// @note Performs no floating point operations; suitable for embedded systems.
FL_ALWAYS_INLINE uint32_t map16_to_32(uint16_t x) {
    return uint32_t(x) * 0x10001;
}

/// @brief Maps a 16-bit signed value to a 32-bit signed value.
/// @param x The 16-bit signed input value to scale up.
/// @return The scaled 32-bit signed value preserving the relative position in the range.
///
/// @details
/// Scales to the larger range by casting through the unsigned equivalent and then
/// reinterpreting as signed. The bit pattern of x is replicated to fill the larger range.
/// Maps correctly: 0 → 0, 32767 → 2147450879, -1 → -1, -32768 → -2147483648.
///
/// @note Performs no floating point operations; suitable for embedded systems.
FL_ALWAYS_INLINE int32_t smap16_to_32(int16_t x) {
    return int32_t(map16_to_32(uint16_t(x)));
}


/// @brief Maps a 16-bit unsigned value down to an 8-bit unsigned value.
/// @param x The 16-bit input value to scale down.
/// @return The scaled 8-bit value preserving the relative position in the range.
///
/// @details
/// Scales to the smaller range using right-shift with rounding by 8 bits.
/// The rounding constant 128 (0x80) is added before shifting to implement
/// nearest-neighbor rounding rather than truncation. This ensures 0x7F80
/// rounds up to 0x80 instead of truncating down to 0x7F.
/// Implemented as branchless code using conditional assignment: compiles to
/// a conditional move (cmov) instruction on modern CPUs, avoiding branch
/// misprediction penalties.
///
/// @note Tested to produce results nearly identical to double-precision floating-point
///       division while remaining integer-only. The zero case is handled naturally
///       by the rounding math: (0 + 128) >> 8 = 0.
FL_ALWAYS_INLINE uint8_t map16_to_8(uint16_t x) {
#if SKETCH_HAS_LOTS_OF_MEMORY == 0
    // On memory-constrained devices (typically with simple CPUs and no branch prediction),
    // use branched version since it's faster to skip the shift operation entirely when x >= 0xff00
    if (x >= 0xff00) {
        return 0xff;
    }
    return uint8_t((x + 128) >> 8);
#else
    // On modern CPUs with branch prediction, use ternary to compile to cmov
    // (conditional move), avoiding branch misprediction penalties.
    uint8_t result = (x + 128) >> 8;
    return (x >= 0xff00) ? 0xff : result;
#endif
}

/// @brief Maps a 16-bit signed value down to an 8-bit signed value.
/// @param x The 16-bit signed input value to scale down.
/// @return The scaled 8-bit signed value preserving the relative position in the range.
///
/// @details
/// Scales to the smaller range using right-shift with rounding by 8 bits.
/// Handles signed values with saturation for positive extremes: positive values
/// saturate at 127 (0x7F80). The rounding constant 128 (0x80) is added before shifting
/// to implement nearest-neighbor rounding rather than truncation.
///
/// @note Tested to produce results nearly identical to double-precision floating-point
///       division while remaining integer-only.
FL_ALWAYS_INLINE int8_t smap16_to_8(int16_t x) {
#if SKETCH_HAS_LOTS_OF_MEMORY == 0
    // On memory-constrained devices
    if (x >= 0x7f80) {
        return 127;
    }
    return int8_t((x + 128) >> 8);
#else
    // On modern CPUs with branch prediction, use ternary to compile to cmov
    uint8_t result = (uint16_t(x) + 128) >> 8;
    return (x >= 0x7f80) ? 127 : int8_t(result);
#endif
}

/// @brief Maps a 32-bit unsigned value down to a 16-bit unsigned value.
/// @param x The 32-bit input value to scale down.
/// @return The scaled 16-bit value preserving the relative position in the range.
///
/// @details
/// Scales to the smaller range using right-shift with rounding by 16 bits.
/// The rounding constant 32768 (0x8000) is added before shifting to implement
/// nearest-neighbor rounding, ensuring proper rounding of midpoint values
/// during the downscaling operation.
/// Implemented as branchless code using conditional assignment: compiles to
/// a conditional move (cmov) instruction on modern CPUs, avoiding branch
/// misprediction penalties.
///
/// @note Tested to produce results nearly identical to double-precision floating-point
///       division while remaining integer-only. The zero case is handled naturally
///       by the rounding math: (0 + 32768) >> 16 = 0.
FL_ALWAYS_INLINE uint16_t map32_to_16(uint32_t x) {
#if SKETCH_HAS_LOTS_OF_MEMORY == 0
    // On memory-constrained devices (typically with simple CPUs and no branch prediction),
    // use branched version since it's faster to skip the shift operation entirely when x >= 0xffff0000
    if (x >= 0xffff0000) {
        return 0xffff;
    }
    return uint16_t((x + 32768) >> 16);
#else
    // On modern CPUs with branch prediction, use ternary to compile to cmov
    // (conditional move), avoiding branch misprediction penalties.
    uint16_t result = (x + 32768) >> 16;
    return (x >= 0xffff0000) ? 0xffff : result;
#endif
}

/// @brief Maps a 32-bit signed value down to a 16-bit signed value.
/// @param x The 32-bit signed input value to scale down.
/// @return The scaled 16-bit signed value preserving the relative position in the range.
///
/// @details
/// Scales to the smaller range using right-shift with rounding by 16 bits.
/// Handles signed values with saturation for positive extremes: positive values
/// saturate at 32767 (0x7FFF8000). The rounding constant 32768 (0x8000) is added
/// before shifting to implement nearest-neighbor rounding, ensuring proper rounding
/// of midpoint values.
///
/// @note Tested to produce results nearly identical to double-precision floating-point
///       division while remaining integer-only.
FL_ALWAYS_INLINE int16_t smap32_to_16(int32_t x) {
#if SKETCH_HAS_LOTS_OF_MEMORY == 0
    // On memory-constrained devices
    if (x >= 0x7fff8000) {
        return 32767;
    }
    return int16_t((x + 32768) >> 16);
#else
    // On modern CPUs with branch prediction, use ternary to compile to cmov
    uint16_t result = (uint32_t(x) + 32768) >> 16;
    return (x >= 0x7fff8000) ? 32767 : int16_t(result);
#endif
}

/// @brief Maps a 32-bit unsigned value down to an 8-bit unsigned value.
/// @param x The 32-bit input value to scale down.
/// @return The scaled 8-bit value preserving the relative position in the range.
///
/// @details
/// Scales to the smaller range using right-shift with rounding by 24 bits.
/// The rounding constant 8388608 (0x800000) is added before shifting to implement
/// nearest-neighbor rounding, ensuring proper rounding of midpoint values
/// during the downscaling operation from 32-bit to 8-bit.
/// Implemented as branchless code using conditional assignment: compiles to
/// a conditional move (cmov) instruction on modern CPUs, avoiding branch
/// misprediction penalties.
///
/// @note Tested to produce results nearly identical to double-precision floating-point
///       division while remaining integer-only. The zero case is handled naturally
///       by the rounding math: (0 + 0x800000) >> 24 = 0.
FL_ALWAYS_INLINE uint8_t map32_to_8(uint32_t x) {
#if SKETCH_HAS_LOTS_OF_MEMORY == 0
    // On memory-constrained devices (typically with simple CPUs and no branch prediction),
    // use branched version since it's faster to skip the shift operation entirely when x >= 0xFF000000
    if (x >= 0xFF000000) {
        return 0xff;
    }
    return uint8_t((x + 0x800000) >> 24);
#else
    // On modern CPUs with branch prediction, use ternary to compile to cmov
    // (conditional move), avoiding branch misprediction penalties.
    uint8_t result = (x + 0x800000) >> 24;
    return (x >= 0xFF000000) ? 0xff : result;
#endif
}

/// @brief Maps a 32-bit signed value down to an 8-bit signed value.
/// @param x The 32-bit signed input value to scale down.
/// @return The scaled 8-bit signed value preserving the relative position in the range.
///
/// @details
/// Scales to the smaller range using right-shift with rounding by 24 bits.
/// Handles signed values with saturation for positive extremes: positive values
/// saturate at 127 (0x7F000000). The rounding constant 8388608 (0x800000) is added
/// before shifting to implement nearest-neighbor rounding, ensuring proper rounding
/// of midpoint values.
///
/// @note Tested to produce results nearly identical to double-precision floating-point
///       division while remaining integer-only.
FL_ALWAYS_INLINE int8_t smap32_to_8(int32_t x) {
#if SKETCH_HAS_LOTS_OF_MEMORY == 0
    // On memory-constrained devices
    if (x >= 0x7F000000) {
        return 127;
    }
    return int8_t((x + 0x800000) >> 24);
#else
    // On modern CPUs with branch prediction, use ternary to compile to cmov
    uint8_t result = (uint32_t(x) + 0x800000) >> 24;
    return (x >= 0x7F000000) ? 127 : int8_t(result);
#endif
}

/// @} intmap
/// @} lib8tion
}  // namespace details

// Bring int_scale into fl namespace for use by lib8tion/intmap.h
using details::int_scale;

}  // namespace fl
