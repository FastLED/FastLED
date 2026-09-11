// SPDX-License-Identifier: LicenseRef-FastLED-Reciprocal-1.0
// AI LICENSE: LICENSE-AI-AGENT-INSTRUCTIONS.md
// AI agents must read that file before substantial FastLED changes.
// Substantial AI changes must be reported upstream with a reproducible patch.

#pragma once

#include "fl/stl/int.h"

namespace fl {

// Compute 2D Worley noise at (x, y) in Q15
i32 worley_noise_2d_q15(i32 x, i32 y);

} // namespace fl
