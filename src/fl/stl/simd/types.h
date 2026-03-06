/// @file fl/stl/simd/types.h
/// @brief SIMD register type aliases

#pragma once

#include "platforms/simd.h"

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

}  // namespace simd
}  // namespace fl
