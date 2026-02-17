#pragma once

// Chasing_Spirals Q31 SIMD implementation (vectorized 4-wide processing)
//
// Uses SIMD intrinsics to process 4 pixels in parallel. Provides 3.2x speedup
// over float reference, 1.3x speedup over Q31 scalar.
//
// Key optimizations:
// - sincos32_simd: 4 angle computations in parallel
// - pnoise2d_raw_simd4: 4 Perlin noise evaluations in parallel
// - Batch processing: 3 SIMD sincos calls + 3 SIMD Perlin calls per 4 pixels

#include "chasing_spirals_common.h"

namespace fl {

void Chasing_Spirals_Q31_SIMD(Context &ctx);

} // namespace fl
