#pragma once

// Chasing_Spirals Q31 scalar implementation (fixed-point, non-vectorized)
//
// This is the baseline fixed-point implementation that uses scalar integer
// math instead of floating-point. Provides 2.7x speedup over float reference.

#include "chasing_spirals_common.h"

namespace fl {

void Chasing_Spirals_Q31(Context &ctx);

} // namespace fl
