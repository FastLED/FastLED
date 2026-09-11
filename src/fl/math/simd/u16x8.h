// SPDX-License-Identifier: LicenseRef-FastLED-Reciprocal-1.0
// AI LICENSE: LICENSE-AI-AGENT-INSTRUCTIONS.md
// AI agents must read that file before substantial FastLED changes.
// Substantial AI changes must be reported upstream with a reproducible patch.

/// @file fl/math/simd/u16x8.h
/// @brief SIMD operations on 8-element uint16_t vectors
///
/// Provides widening/narrowing operations between u8x16 and u16x8 for
/// convolution kernels that need intermediate 16-bit precision.

#pragma once

#include "fl/math/simd/types.h"

namespace fl {
namespace simd {

//==============================================================================
// Widening Operations (u8x16 → u16x8)
//==============================================================================

/// Zero-extend the low 8 bytes of a u8x16 vector to u16x8
/// @param vec Input u8x16 vector
/// @return u16x8 with elements [0..7] widened from vec's bytes [0..7]
using platforms::widen_lo_u8_to_u16;  // ok bare using

/// Zero-extend the high 8 bytes of a u8x16 vector to u16x8
/// @param vec Input u8x16 vector
/// @return u16x8 with elements [0..7] widened from vec's bytes [8..15]
using platforms::widen_hi_u8_to_u16;  // ok bare using

//==============================================================================
// Narrowing Operations (u16x8 → u8x16)
//==============================================================================

/// Pack two u16x8 vectors into one u8x16 with unsigned saturation
/// Values > 255 are clamped to 255.
/// @param lo Low 8 elements (become bytes [0..7])
/// @param hi High 8 elements (become bytes [8..15])
/// @return u8x16 packed result
using platforms::narrow_u16_to_u8;  // ok bare using

//==============================================================================
// Arithmetic Operations
//==============================================================================

/// Element-wise addition of two u16x8 vectors (wrapping)
/// @param a First operand (8 uint16_t)
/// @param b Second operand (8 uint16_t)
/// @return Element-wise sum
using platforms::add_u16_8;  // ok bare using

/// Element-wise multiply, keep low 16 bits: (a * b) & 0xFFFF
/// @param a First operand (8 uint16_t)
/// @param b Second operand (8 uint16_t)
/// @return Low 16 bits of each product
using platforms::mullo_u16_8;  // ok bare using

//==============================================================================
// Shift Operations
//==============================================================================

/// Logical right shift each u16 element by immediate count
/// @param vec Input vector (8 uint16_t)
/// @param shift Number of bits to shift right (0-15)
/// @return Shifted result
using platforms::srli_u16_8;  // ok bare using

//==============================================================================
// Broadcast Operations
//==============================================================================

/// Broadcast a single u16 value to all 8 lanes
/// @param value Value to broadcast
/// @return u16x8 with all lanes set to value
using platforms::set1_u16_8;  // ok bare using

}  // namespace simd
}  // namespace fl
