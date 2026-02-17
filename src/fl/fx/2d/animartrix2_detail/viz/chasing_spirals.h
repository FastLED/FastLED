#pragma once

// Chasing_Spirals visualizer declarations
// Three variants: float reference, Q31 fixed-point, Q31 SIMD
//
// - Chasing_Spirals: Original floating-point reference implementation
// - Chasing_Spirals_Q31: Fixed-point scalar (2.7x speedup over float)
// - Chasing_Spirals_Q31_SIMD: SIMD vectorized 4-wide (3.2x speedup over float)

#include "fl/fx/2d/animartrix2_detail/context.h"

namespace fl {

// Float reference implementation (baseline)
void Chasing_Spirals(Context &ctx);

// Q31 fixed-point scalar implementation (2.7x speedup)
void Chasing_Spirals_Q31(Context &ctx);

// Q31 SIMD implementation (3.2x speedup, optimization target)
void Chasing_Spirals_Q31_SIMD(Context &ctx);

}  // namespace fl
