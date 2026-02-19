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
using simd_u8x16 = platforms::simd_u8x16;
using simd_u32x4 = platforms::simd_u32x4;
using simd_f32x4 = platforms::simd_f32x4;

//==============================================================================
// Atomic Load/Store Operations (bring platform implementations into public namespace)
//==============================================================================

/// Load 16 uint8_t values from memory into a SIMD register
/// @param ptr Source pointer (unaligned access supported)
/// @return SIMD register containing 16 uint8_t values
using platforms::load_u8_16;

/// Store 16 uint8_t values from SIMD register to memory
/// @param ptr Destination pointer (unaligned access supported)
/// @param vec SIMD register containing 16 uint8_t values
using platforms::store_u8_16;

/// Load 4 uint32_t values from memory into a SIMD register
/// @param ptr Source pointer (unaligned access supported)
/// @return SIMD register containing 4 uint32_t values
using platforms::load_u32_4;

/// Store 4 uint32_t values from SIMD register to memory
/// @param ptr Destination pointer (unaligned access supported)
/// @param vec SIMD register containing 4 uint32_t values
using platforms::store_u32_4;

//==============================================================================
// Atomic Arithmetic Operations (bring platform implementations into public namespace)
//==============================================================================

/// Saturating add: (a + b) clamped to [0, 255]
/// @param a First operand (16 uint8_t)
/// @param b Second operand (16 uint8_t)
/// @return Result of saturating addition
using platforms::add_sat_u8_16;

/// Scale uint8_t vector by factor: (vec * scale) / 256
/// @param vec Input vector (16 uint8_t)
/// @param scale Scale factor (0-255, where 256 = 1.0)
/// @return Scaled result
using platforms::scale_u8_16;

/// Broadcast uint32 value to all 4 lanes
/// @param value Value to broadcast
/// @return SIMD register with value replicated 4 times
using platforms::set1_u32_4;

/// Saturating subtract: (a - b) clamped to [0, 255]
/// @param a First operand (16 uint8_t)
/// @param b Second operand (16 uint8_t)
/// @return Result of saturating subtraction
using platforms::sub_sat_u8_16;

/// Average two uint8_t vectors: (a + b) / 2 (rounds down)
/// @param a First operand (16 uint8_t)
/// @param b Second operand (16 uint8_t)
/// @return Element-wise average
using platforms::avg_u8_16;

/// Average two uint8_t vectors with rounding: (a + b + 1) / 2
/// @param a First operand (16 uint8_t)
/// @param b Second operand (16 uint8_t)
/// @return Element-wise average with rounding
using platforms::avg_round_u8_16;

/// Element-wise minimum of two uint8_t vectors
/// @param a First operand (16 uint8_t)
/// @param b Second operand (16 uint8_t)
/// @return Element-wise minimum
using platforms::min_u8_16;

/// Element-wise maximum of two uint8_t vectors
/// @param a First operand (16 uint8_t)
/// @param b Second operand (16 uint8_t)
/// @return Element-wise maximum
using platforms::max_u8_16;

/// Bitwise AND of two uint8_t vectors
/// @param a First operand (16 uint8_t)
/// @param b Second operand (16 uint8_t)
/// @return Bitwise AND result
using platforms::and_u8_16;

/// Bitwise OR of two uint8_t vectors
/// @param a First operand (16 uint8_t)
/// @param b Second operand (16 uint8_t)
/// @return Bitwise OR result
using platforms::or_u8_16;

/// Bitwise XOR of two uint8_t vectors
/// @param a First operand (16 uint8_t)
/// @param b Second operand (16 uint8_t)
/// @return Bitwise XOR result
using platforms::xor_u8_16;

/// Bitwise AND-NOT: ~a & b
/// @param a First operand (16 uint8_t) - will be inverted
/// @param b Second operand (16 uint8_t)
/// @return Bitwise AND-NOT result
using platforms::andnot_u8_16;

/// Blend two uint8_t vectors: result = a + ((b - a) * amount) / 256
/// @param a First input vector (16 uint8_t)
/// @param b Second input vector (16 uint8_t)
/// @param amount Blend factor (0 = all a, 255 = all b)
/// @return Blended result
using platforms::blend_u8_16;

//==============================================================================
// Floating Point Operations (bring platform implementations into public namespace)
//==============================================================================

/// Load 4 float32 values from memory into a SIMD register
/// @param ptr Source pointer (aligned access preferred)
/// @return SIMD register containing 4 float32 values
using platforms::load_f32_4;

/// Store 4 float32 values from SIMD register to memory
/// @param ptr Destination pointer (aligned access preferred)
/// @param vec SIMD register containing 4 float32 values
using platforms::store_f32_4;

/// Broadcast float32 value to all 4 lanes
/// @param value Value to broadcast
/// @return SIMD register with value replicated 4 times
using platforms::set1_f32_4;

/// Add two float32 vectors
/// @param a First operand (4 float32)
/// @param b Second operand (4 float32)
/// @return Element-wise addition result
using platforms::add_f32_4;

/// Subtract two float32 vectors
/// @param a First operand (4 float32)
/// @param b Second operand (4 float32)
/// @return Element-wise subtraction result
using platforms::sub_f32_4;

/// Multiply two float32 vectors
/// @param a First operand (4 float32)
/// @param b Second operand (4 float32)
/// @return Element-wise multiplication result
using platforms::mul_f32_4;

/// Divide two float32 vectors
/// @param a First operand (4 float32)
/// @param b Second operand (4 float32)
/// @return Element-wise division result
using platforms::div_f32_4;

/// Square root of float32 vector
/// @param vec Input vector (4 float32)
/// @return Element-wise square root
using platforms::sqrt_f32_4;

/// Element-wise minimum of two float32 vectors
/// @param a First operand (4 float32)
/// @param b Second operand (4 float32)
/// @return Element-wise minimum
using platforms::min_f32_4;

/// Element-wise maximum of two float32 vectors
/// @param a First operand (4 float32)
/// @param b Second operand (4 float32)
/// @return Element-wise maximum
using platforms::max_f32_4;

//==============================================================================
// Int32 SIMD Operations
//==============================================================================

/// Bitwise XOR of two uint32 vectors
/// @param a First operand (4 uint32)
/// @param b Second operand (4 uint32)
/// @return Bitwise XOR result
using platforms::xor_u32_4;

/// Add two int32 vectors (treating u32x4 as i32x4)
/// @param a First operand (4 int32)
/// @param b Second operand (4 int32)
/// @return Element-wise addition result
using platforms::add_i32_4;

/// Subtract two int32 vectors (treating u32x4 as i32x4)
/// @param a First operand (4 int32)
/// @param b Second operand (4 int32)
/// @return Element-wise subtraction result
using platforms::sub_i32_4;

/// Multiply int32 and return high 32 bits (for Q16.16 fixed-point)
/// @param a First operand (4 int32)
/// @param b Second operand (4 int32)
/// @return High 32 bits of 64-bit product, shifted right by 16
using platforms::mulhi_i32_4;

/// Multiply uint32 and return high 32 bits (for Q16.16 fixed-point, unsigned)
/// @param a First operand (4 uint32)
/// @param b Second operand (4 uint32)
/// @return High 32 bits of 64-bit unsigned product, shifted right by 16
/// On SSE2 this is 8 ops vs 14 for signed mulhi_i32_4
using platforms::mulhi_u32_4;

/// Multiply signed i32 by unsigned-positive u32, return >> 16 (Q16.16 fixed-point)
/// @param a First operand (4 int32, signed)
/// @param b Second operand (4 uint32, MUST be non-negative)
/// @return ((i64)(i32)a * (u64)(u32)b) >> 16 for each lane
/// On SSE2 this is 11 ops (vs 14 for mulhi_i32_4) — skips b's sign correction.
/// Produces bit-identical results to mulhi_i32_4 when b >= 0.
using platforms::mulhi_su32_4;

/// Shift right logical (zero-fill) - for unsigned angle decomposition
/// @param vec Input vector (4 uint32)
/// @param shift Number of bits to shift right
/// @return Vector shifted right by specified bits
using platforms::srl_u32_4;

/// Shift left logical (zero-fill) - for fixed-point format conversion
/// @param vec Input vector (4 uint32)
/// @param shift Number of bits to shift left
/// @return Vector shifted left by specified bits
using platforms::sll_u32_4;

/// Shift right arithmetic (sign-extend) - for signed fixed-point math
/// @param vec Input vector (4 int32 stored as uint32)
/// @param shift Number of bits to shift right
/// @return Vector shifted right with sign extension
using platforms::sra_i32_4;

/// Bitwise OR of two uint32 vectors
/// @param a First operand (4 uint32)
/// @param b Second operand (4 uint32)
/// @return Bitwise OR result
using platforms::or_u32_4;

/// Bitwise AND of two uint32 vectors
/// @param a First operand (4 uint32)
/// @param b Second operand (4 uint32)
/// @return Bitwise AND result
using platforms::and_u32_4;

/// Signed element-wise minimum of two int32 vectors
/// @param a First operand (4 int32 stored as uint32)
/// @param b Second operand (4 int32 stored as uint32)
/// @return Element-wise signed minimum
using platforms::min_i32_4;

/// Signed element-wise maximum of two int32 vectors
/// @param a First operand (4 int32 stored as uint32)
/// @param b Second operand (4 int32 stored as uint32)
/// @return Element-wise signed maximum
using platforms::max_i32_4;

/// Signed multiply high 32 bits: ((i64)a * (i64)b) >> 32, for each of 4 lanes
/// Classical mulhi_epi32 — high 32 bits of the signed 64-bit product.
/// Key identity: (Q31 * Q16.16) >> 31 == mulhi32_i32_4(a, b) << 1
/// Contrast with mulhi_i32_4 which returns >> 16 (Q16.16 fixed-point arithmetic).
using platforms::mulhi32_i32_4;

/// Extract a single u32 lane from a SIMD vector
/// @param vec Input vector (4 uint32)
/// @param lane Lane index (0-3)
/// @return The u32 value at the specified lane
using platforms::extract_u32_4;

/// Interleave low 32-bit elements: {a0, b0, a1, b1}
using platforms::unpacklo_u32_4;

/// Interleave high 32-bit elements: {a2, b2, a3, b3}
using platforms::unpackhi_u32_4;

/// Interleave low 64-bit halves (as u32x4): {a0, a1, b0, b1}
using platforms::unpacklo_u64_as_u32_4;

/// Interleave high 64-bit halves (as u32x4): {a2, a3, b2, b3}
using platforms::unpackhi_u64_as_u32_4;

}  // namespace simd
}  // namespace fl
