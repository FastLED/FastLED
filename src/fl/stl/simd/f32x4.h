/// @file fl/stl/simd/f32x4.h
/// @brief SIMD operations on 4-element float32 vectors

#pragma once

#include "fl/stl/simd/types.h"

namespace fl {
namespace simd {

//==============================================================================
// Load/Store Operations
//==============================================================================

/// Load 4 float32 values from memory into a SIMD register
/// @param ptr Source pointer (aligned access preferred)
/// @return SIMD register containing 4 float32 values
using platforms::load_f32_4;  // ok bare using

/// Store 4 float32 values from SIMD register to memory
/// @param ptr Destination pointer (aligned access preferred)
/// @param vec SIMD register containing 4 float32 values
using platforms::store_f32_4;  // ok bare using

//==============================================================================
// Broadcast/Set Operations
//==============================================================================

/// Broadcast float32 value to all 4 lanes
/// @param value Value to broadcast
/// @return SIMD register with value replicated 4 times
using platforms::set1_f32_4;  // ok bare using

//==============================================================================
// Arithmetic Operations
//==============================================================================

/// Add two float32 vectors
/// @param a First operand (4 float32)
/// @param b Second operand (4 float32)
/// @return Element-wise addition result
using platforms::add_f32_4;  // ok bare using

/// Subtract two float32 vectors
/// @param a First operand (4 float32)
/// @param b Second operand (4 float32)
/// @return Element-wise subtraction result
using platforms::sub_f32_4;  // ok bare using

/// Multiply two float32 vectors
/// @param a First operand (4 float32)
/// @param b Second operand (4 float32)
/// @return Element-wise multiplication result
using platforms::mul_f32_4;  // ok bare using

/// Divide two float32 vectors
/// @param a First operand (4 float32)
/// @param b Second operand (4 float32)
/// @return Element-wise division result
using platforms::div_f32_4;  // ok bare using

/// Square root of float32 vector
/// @param vec Input vector (4 float32)
/// @return Element-wise square root
using platforms::sqrt_f32_4;  // ok bare using

//==============================================================================
// Min/Max Operations
//==============================================================================

/// Element-wise minimum of two float32 vectors
/// @param a First operand (4 float32)
/// @param b Second operand (4 float32)
/// @return Element-wise minimum
using platforms::min_f32_4;  // ok bare using

/// Element-wise maximum of two float32 vectors
/// @param a First operand (4 float32)
/// @param b Second operand (4 float32)
/// @return Element-wise maximum
using platforms::max_f32_4;  // ok bare using

}  // namespace simd
}  // namespace fl
