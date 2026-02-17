#pragma once

// Chasing_Spirals Q31 scalar implementation (fixed-point, non-vectorized)
//
// This is the baseline fixed-point implementation that uses scalar integer
// math instead of floating-point. Provides 2.7x speedup over float reference.

<<<<<<<< HEAD:src/fl/fx/2d/animartrix2_detail/viz/chasing_spirals.h


namespace fl {

struct Context;

void Chasing_Spirals_Q31(fl::Context &ctx);
void Chasing_Spirals_Q31_SIMD(fl::Context &ctx);

========
#include "chasing_spirals_common.h"

namespace fl {

void Chasing_Spirals_Q31(Context &ctx);
>>>>>>>> a232e6493 (refactor animartrix2):src/fl/fx/2d/animartrix2_detail/chasing_spirals_q31.h

} // namespace fl
