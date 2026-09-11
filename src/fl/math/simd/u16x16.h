// SPDX-License-Identifier: LicenseRef-FastLED-Reciprocal-1.0
// AI LICENSE: LICENSE-AI-AGENT-INSTRUCTIONS.md
// AI agents must read that file before substantial FastLED changes.
// Substantial AI changes must be reported upstream with a reproducible patch.

/// @file fl/math/simd/u16x16.h
/// @brief SIMD operations on 16-element uint16_t vectors (256-bit)

#pragma once

#include "fl/math/simd/types.h"

namespace fl {
namespace simd {

//==============================================================================
// u16x16 Arithmetic Operations
//==============================================================================

/// Element-wise addition of u16x16 vectors (wrapping)
using platforms::add_u16_16;  // ok bare using

/// Element-wise multiply, keep low 16 bits
using platforms::mullo_u16_16;  // ok bare using

/// Logical right shift each u16 element
using platforms::srli_u16_16;  // ok bare using

/// Broadcast u16 value to all 16 lanes
using platforms::set1_u16_16;  // ok bare using

}  // namespace simd
}  // namespace fl
