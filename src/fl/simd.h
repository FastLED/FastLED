#pragma once

/// @file simd.h
/// SIMD-accelerated atomic operations for FastLED
///
/// Provides register-level atomic operations for composing cache-friendly SIMD pipelines.
///
/// Design principles:
/// - Register types (simd_u8x16, simd_u32x4) work across platforms
/// - Atomic operations only (load, store, arithmetic) - no bulk operations
/// - Zero overhead: everything inlines to native SIMD via FASTLED_FORCE_INLINE
/// - Cache-friendly: compose operations in single pass
/// - Platform delegation: implementations in fl::simd::platform namespace
///
/// Example (cache-efficient composition):
/// @code
///   using namespace fl::simd;
///   for (size_t i = 0; i < count; i += 16) {
///       auto v = load_u8_16(&src[i]);
///       v = scale_u8_16(v, 128);              // Scale by 0.5
///       auto w = load_u8_16(&other[i]);
///       v = add_sat_u8_16(v, w);              // Add with saturation
///       store_u8_16(&dst[i], v);
///   }
/// @endcode

// Platform dispatch header - delegates to platform-specific SIMD implementations
// using the coarse-to-fine pattern. Uses scalar fallback for non-SIMD platforms.
#include "platforms/simd.h"  // ok platform headers

namespace fl {
namespace simd {

//==============================================================================
// SIMD Register Types (from platform implementation)
//==============================================================================

// Platform implementations define these in fl::simd::platform namespace:
// - simd_u8x16: 16-element uint8_t vector (128 bits)
// - simd_u32x4: 4-element uint32_t vector (128 bits)
// - simd_f32x4: 4-element float32 vector (128 bits)

// Expose platform types in public fl::simd namespace
using simd_u8x16 = platform::simd_u8x16;
using simd_u32x4 = platform::simd_u32x4;
using simd_f32x4 = platform::simd_f32x4;

//==============================================================================
// Atomic Load/Store Operations (bring platform implementations into public namespace)
//==============================================================================

/// Load 16 uint8_t values from memory into a SIMD register
/// @param ptr Source pointer (unaligned access supported)
/// @return SIMD register containing 16 uint8_t values
using platform::load_u8_16;

/// Store 16 uint8_t values from SIMD register to memory
/// @param ptr Destination pointer (unaligned access supported)
/// @param vec SIMD register containing 16 uint8_t values
using platform::store_u8_16;

/// Load 4 uint32_t values from memory into a SIMD register
/// @param ptr Source pointer (unaligned access supported)
/// @return SIMD register containing 4 uint32_t values
using platform::load_u32_4;

/// Store 4 uint32_t values from SIMD register to memory
/// @param ptr Destination pointer (unaligned access supported)
/// @param vec SIMD register containing 4 uint32_t values
using platform::store_u32_4;

//==============================================================================
// Atomic Arithmetic Operations (bring platform implementations into public namespace)
//==============================================================================

/// Saturating add: (a + b) clamped to [0, 255]
/// @param a First operand (16 uint8_t)
/// @param b Second operand (16 uint8_t)
/// @return Result of saturating addition
using platform::add_sat_u8_16;

/// Scale uint8_t vector by factor: (vec * scale) / 256
/// @param vec Input vector (16 uint8_t)
/// @param scale Scale factor (0-255, where 256 = 1.0)
/// @return Scaled result
using platform::scale_u8_16;

/// Broadcast uint32 value to all 4 lanes
/// @param value Value to broadcast
/// @return SIMD register with value replicated 4 times
using platform::set1_u32_4;

/// Saturating subtract: (a - b) clamped to [0, 255]
/// @param a First operand (16 uint8_t)
/// @param b Second operand (16 uint8_t)
/// @return Result of saturating subtraction
using platform::sub_sat_u8_16;

/// Average two uint8_t vectors: (a + b) / 2 (rounds down)
/// @param a First operand (16 uint8_t)
/// @param b Second operand (16 uint8_t)
/// @return Element-wise average
using platform::avg_u8_16;

/// Average two uint8_t vectors with rounding: (a + b + 1) / 2
/// @param a First operand (16 uint8_t)
/// @param b Second operand (16 uint8_t)
/// @return Element-wise average with rounding
using platform::avg_round_u8_16;

/// Element-wise minimum of two uint8_t vectors
/// @param a First operand (16 uint8_t)
/// @param b Second operand (16 uint8_t)
/// @return Element-wise minimum
using platform::min_u8_16;

/// Element-wise maximum of two uint8_t vectors
/// @param a First operand (16 uint8_t)
/// @param b Second operand (16 uint8_t)
/// @return Element-wise maximum
using platform::max_u8_16;

/// Bitwise AND of two uint8_t vectors
/// @param a First operand (16 uint8_t)
/// @param b Second operand (16 uint8_t)
/// @return Bitwise AND result
using platform::and_u8_16;

/// Bitwise OR of two uint8_t vectors
/// @param a First operand (16 uint8_t)
/// @param b Second operand (16 uint8_t)
/// @return Bitwise OR result
using platform::or_u8_16;

/// Bitwise XOR of two uint8_t vectors
/// @param a First operand (16 uint8_t)
/// @param b Second operand (16 uint8_t)
/// @return Bitwise XOR result
using platform::xor_u8_16;

/// Bitwise AND-NOT: ~a & b
/// @param a First operand (16 uint8_t) - will be inverted
/// @param b Second operand (16 uint8_t)
/// @return Bitwise AND-NOT result
using platform::andnot_u8_16;

/// Blend two uint8_t vectors: result = a + ((b - a) * amount) / 256
/// @param a First input vector (16 uint8_t)
/// @param b Second input vector (16 uint8_t)
/// @param amount Blend factor (0 = all a, 255 = all b)
/// @return Blended result
using platform::blend_u8_16;

//==============================================================================
// Floating Point Operations (bring platform implementations into public namespace)
//==============================================================================

/// Load 4 float32 values from memory into a SIMD register
/// @param ptr Source pointer (aligned access preferred)
/// @return SIMD register containing 4 float32 values
using platform::load_f32_4;

/// Store 4 float32 values from SIMD register to memory
/// @param ptr Destination pointer (aligned access preferred)
/// @param vec SIMD register containing 4 float32 values
using platform::store_f32_4;

/// Broadcast float32 value to all 4 lanes
/// @param value Value to broadcast
/// @return SIMD register with value replicated 4 times
using platform::set1_f32_4;

/// Add two float32 vectors
/// @param a First operand (4 float32)
/// @param b Second operand (4 float32)
/// @return Element-wise addition result
using platform::add_f32_4;

/// Subtract two float32 vectors
/// @param a First operand (4 float32)
/// @param b Second operand (4 float32)
/// @return Element-wise subtraction result
using platform::sub_f32_4;

/// Multiply two float32 vectors
/// @param a First operand (4 float32)
/// @param b Second operand (4 float32)
/// @return Element-wise multiplication result
using platform::mul_f32_4;

/// Divide two float32 vectors
/// @param a First operand (4 float32)
/// @param b Second operand (4 float32)
/// @return Element-wise division result
using platform::div_f32_4;

/// Square root of float32 vector
/// @param vec Input vector (4 float32)
/// @return Element-wise square root
using platform::sqrt_f32_4;

/// Element-wise minimum of two float32 vectors
/// @param a First operand (4 float32)
/// @param b Second operand (4 float32)
/// @return Element-wise minimum
using platform::min_f32_4;

/// Element-wise maximum of two float32 vectors
/// @param a First operand (4 float32)
/// @param b Second operand (4 float32)
/// @return Element-wise maximum
using platform::max_f32_4;

}  // namespace simd
}  // namespace fl
