#pragma once

// Chasing_Spirals Q31 scalar implementation (fixed-point, non-vectorized)
//
// This is the baseline fixed-point implementation that uses scalar integer
// math instead of floating-point. Provides 2.7x speedup over float reference.



namespace fl {

struct Context;

void Chasing_Spirals_Q31(fl::Context &ctx);
void Chasing_Spirals_Q31_SIMD(fl::Context &ctx);


} // namespace fl
