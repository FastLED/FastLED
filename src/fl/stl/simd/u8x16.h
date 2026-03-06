/// @file fl/stl/simd/u8x16.h
/// @brief SIMD operations on 16-element uint8_t vectors

#pragma once

#include "fl/stl/simd/types.h"

namespace fl {
namespace simd {

//==============================================================================
// Atomic Load/Store Operations
//==============================================================================

/// Load 16 uint8_t values from memory into a SIMD register
/// @param ptr Source pointer (unaligned access supported)
/// @return SIMD register containing 16 uint8_t values
using platforms::load_u8_16;  // ok bare using

/// Store 16 uint8_t values from SIMD register to memory
/// @param ptr Destination pointer (unaligned access supported)
/// @param vec SIMD register containing 16 uint8_t values
using platforms::store_u8_16;  // ok bare using

//==============================================================================
// Saturating Arithmetic Operations
//==============================================================================

/// Saturating add: (a + b) clamped to [0, 255]
/// @param a First operand (16 uint8_t)
/// @param b Second operand (16 uint8_t)
/// @return Result of saturating addition
using platforms::add_sat_u8_16;  // ok bare using

/// Saturating subtract: (a - b) clamped to [0, 255]
/// @param a First operand (16 uint8_t)
/// @param b Second operand (16 uint8_t)
/// @return Result of saturating subtraction
using platforms::sub_sat_u8_16;  // ok bare using

/// Scale uint8_t vector by factor: (vec * scale) / 256
/// @param vec Input vector (16 uint8_t)
/// @param scale Scale factor (0-255, where 256 = 1.0)
/// @return Scaled result
using platforms::scale_u8_16;  // ok bare using

//==============================================================================
// Average Operations
//==============================================================================

/// Average two uint8_t vectors: (a + b) / 2 (rounds down)
/// @param a First operand (16 uint8_t)
/// @param b Second operand (16 uint8_t)
/// @return Element-wise average
using platforms::avg_u8_16;  // ok bare using

/// Average two uint8_t vectors with rounding: (a + b + 1) / 2
/// @param a First operand (16 uint8_t)
/// @param b Second operand (16 uint8_t)
/// @return Element-wise average with rounding
using platforms::avg_round_u8_16;  // ok bare using

//==============================================================================
// Min/Max Operations
//==============================================================================

/// Element-wise minimum of two uint8_t vectors
/// @param a First operand (16 uint8_t)
/// @param b Second operand (16 uint8_t)
/// @return Element-wise minimum
using platforms::min_u8_16;  // ok bare using

/// Element-wise maximum of two uint8_t vectors
/// @param a First operand (16 uint8_t)
/// @param b Second operand (16 uint8_t)
/// @return Element-wise maximum
using platforms::max_u8_16;  // ok bare using

//==============================================================================
// Bitwise Operations
//==============================================================================

/// Bitwise AND of two uint8_t vectors
/// @param a First operand (16 uint8_t)
/// @param b Second operand (16 uint8_t)
/// @return Bitwise AND result
using platforms::and_u8_16;  // ok bare using

/// Bitwise OR of two uint8_t vectors
/// @param a First operand (16 uint8_t)
/// @param b Second operand (16 uint8_t)
/// @return Bitwise OR result
using platforms::or_u8_16;  // ok bare using

/// Bitwise XOR of two uint8_t vectors
/// @param a First operand (16 uint8_t)
/// @param b Second operand (16 uint8_t)
/// @return Bitwise XOR result
using platforms::xor_u8_16;  // ok bare using

/// Bitwise AND-NOT: ~a & b
/// @param a First operand (16 uint8_t) - will be inverted
/// @param b Second operand (16 uint8_t)
/// @return Bitwise AND-NOT result
using platforms::andnot_u8_16;  // ok bare using

//==============================================================================
// Blend Operations
//==============================================================================

/// Blend two uint8_t vectors: result = a + ((b - a) * amount) / 256
/// @param a First input vector (16 uint8_t)
/// @param b Second input vector (16 uint8_t)
/// @param amount Blend factor (0 = all a, 255 = all b)
/// @return Blended result
using platforms::blend_u8_16;  // ok bare using

}  // namespace simd
}  // namespace fl
