#pragma once

#include "fl/stl/int.h"
#include "fl/stl/noexcept.h"

namespace fl {

// Compute 2D Worley noise at (x, y) in Q15
i32 worley_noise_2d_q15(i32 x, i32 y) FL_NO_EXCEPT;

} // namespace fl
