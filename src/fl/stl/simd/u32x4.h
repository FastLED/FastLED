/// @file fl/stl/simd/u32x4.h
/// @brief SIMD operations on 4-element uint32_t/int32_t vectors

#pragma once

#include "fl/stl/simd/types.h"

namespace fl {
namespace simd {

//==============================================================================
// Load/Store Operations
//==============================================================================

/// Load 4 uint32_t values from memory into a SIMD register
/// @param ptr Source pointer (unaligned access supported)
/// @return SIMD register containing 4 uint32_t values
using platforms::load_u32_4;  // ok bare using

/// Load 4 uint32_t values from 16-byte aligned memory into a SIMD register.
/// Uses _mm_load_si128 on SSE2 (faster than loadu on older cores).
/// @param ptr Source pointer — MUST be 16-byte aligned (assert with FL_ASSUME_ALIGNED)
/// @return SIMD register containing 4 uint32_t values
using platforms::load_u32_4_aligned;  // ok bare using

/// Store 4 uint32_t values from SIMD register to memory
/// @param ptr Destination pointer (unaligned access supported)
/// @param vec SIMD register containing 4 uint32_t values
using platforms::store_u32_4;  // ok bare using

/// Store 4 uint32_t values from SIMD register to 16-byte aligned memory.
/// Uses _mm_store_si128 on SSE2 (faster than storeu on older cores).
/// @param ptr Destination pointer — MUST be 16-byte aligned (assert with FL_ASSUME_ALIGNED)
/// @param vec SIMD register containing 4 uint32_t values
using platforms::store_u32_4_aligned;  // ok bare using

//==============================================================================
// Broadcast/Set Operations
//==============================================================================

/// Broadcast uint32 value to all 4 lanes
/// @param value Value to broadcast
/// @return SIMD register with value replicated 4 times
using platforms::set1_u32_4;  // ok bare using

/// Construct a simd_u32x4 from 4 individual u32 values
/// @param a Lane 0 value
/// @param b Lane 1 value
/// @param c Lane 2 value
/// @param d Lane 3 value
/// @return SIMD register {a, b, c, d}
using platforms::set_u32_4;  // ok bare using

//==============================================================================
// Bitwise Operations (uint32)
//==============================================================================

/// Bitwise XOR of two uint32 vectors
/// @param a First operand (4 uint32)
/// @param b Second operand (4 uint32)
/// @return Bitwise XOR result
using platforms::xor_u32_4;  // ok bare using

/// Bitwise OR of two uint32 vectors
/// @param a First operand (4 uint32)
/// @param b Second operand (4 uint32)
/// @return Bitwise OR result
using platforms::or_u32_4;  // ok bare using

/// Bitwise AND of two uint32 vectors
/// @param a First operand (4 uint32)
/// @param b Second operand (4 uint32)
/// @return Bitwise AND result
using platforms::and_u32_4;  // ok bare using

//==============================================================================
// Signed Integer Arithmetic (treating u32x4 as i32x4)
//==============================================================================

/// Add two int32 vectors (treating u32x4 as i32x4)
/// @param a First operand (4 int32)
/// @param b Second operand (4 int32)
/// @return Element-wise addition result
using platforms::add_i32_4;  // ok bare using

/// Subtract two int32 vectors (treating u32x4 as i32x4)
/// @param a First operand (4 int32)
/// @param b Second operand (4 int32)
/// @return Element-wise subtraction result
using platforms::sub_i32_4;  // ok bare using

/// Multiply int32 and return high 32 bits (for Q16.16 fixed-point)
/// @param a First operand (4 int32)
/// @param b Second operand (4 int32)
/// @return High 32 bits of 64-bit product, shifted right by 16
using platforms::mulhi_i32_4;  // ok bare using

/// Multiply uint32 and return high 32 bits (for Q16.16 fixed-point, unsigned)
/// @param a First operand (4 uint32)
/// @param b Second operand (4 uint32)
/// @return High 32 bits of 64-bit unsigned product, shifted right by 16
/// On SSE2 this is 8 ops vs 14 for signed mulhi_i32_4
using platforms::mulhi_u32_4;  // ok bare using

/// Multiply signed i32 by unsigned-positive u32, return >> 16 (Q16.16 fixed-point)
/// @param a First operand (4 int32, signed)
/// @param b Second operand (4 uint32, MUST be non-negative)
/// @return ((i64)(i32)a * (u64)(u32)b) >> 16 for each lane
/// On SSE2 this is 11 ops (vs 14 for mulhi_i32_4) — skips b's sign correction.
/// Produces bit-identical results to mulhi_i32_4 when b >= 0.
using platforms::mulhi_su32_4;  // ok bare using

/// Signed multiply high 32 bits: ((i64)a * (i64)b) >> 32, for each of 4 lanes
/// Classical mulhi_epi32 — high 32 bits of the signed 64-bit product.
/// Key identity: (Q31 * Q16.16) >> 31 == mulhi32_i32_4(a, b) << 1
/// Contrast with mulhi_i32_4 which returns >> 16 (Q16.16 fixed-point arithmetic).
using platforms::mulhi32_i32_4;  // ok bare using

//==============================================================================
// Shift Operations
//==============================================================================

/// Shift right logical (zero-fill) - for unsigned angle decomposition
/// @param vec Input vector (4 uint32)
/// @param shift Number of bits to shift right
/// @return Vector shifted right by specified bits
using platforms::srl_u32_4;  // ok bare using

/// Shift left logical (zero-fill) - for fixed-point format conversion
/// @param vec Input vector (4 uint32)
/// @param shift Number of bits to shift left
/// @return Vector shifted left by specified bits
using platforms::sll_u32_4;  // ok bare using

/// Shift right arithmetic (sign-extend) - for signed fixed-point math
/// @param vec Input vector (4 int32 stored as uint32)
/// @param shift Number of bits to shift right
/// @return Vector shifted right with sign extension
using platforms::sra_i32_4;  // ok bare using

//==============================================================================
// Min/Max Operations (signed)
//==============================================================================

/// Signed element-wise minimum of two int32 vectors
/// @param a First operand (4 int32 stored as uint32)
/// @param b Second operand (4 int32 stored as uint32)
/// @return Element-wise signed minimum
using platforms::min_i32_4;  // ok bare using

/// Signed element-wise maximum of two int32 vectors
/// @param a First operand (4 int32 stored as uint32)
/// @param b Second operand (4 int32 stored as uint32)
/// @return Element-wise signed maximum
using platforms::max_i32_4;  // ok bare using

//==============================================================================
// Element Access and Interleaving
//==============================================================================

/// Extract a single u32 lane from a SIMD vector
/// @param vec Input vector (4 uint32)
/// @param lane Lane index (0-3)
/// @return The u32 value at the specified lane
using platforms::extract_u32_4;  // ok bare using

/// Interleave low 32-bit elements: {a0, b0, a1, b1}
using platforms::unpacklo_u32_4;  // ok bare using

/// Interleave high 32-bit elements: {a2, b2, a3, b3}
using platforms::unpackhi_u32_4;  // ok bare using

/// Interleave low 64-bit halves (as u32x4): {a0, a1, b0, b1}
using platforms::unpacklo_u64_as_u32_4;  // ok bare using

/// Interleave high 64-bit halves (as u32x4): {a2, a3, b2, b3}
using platforms::unpackhi_u64_as_u32_4;  // ok bare using

}  // namespace simd
}  // namespace fl
