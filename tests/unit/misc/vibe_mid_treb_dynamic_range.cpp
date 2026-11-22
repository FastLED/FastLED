// Repro for GitHub issue #2193 comment 4019752955:
// Vibe mid/treble relative levels stuck at 1.00
//
// The bug: For continuous signals (mid/treble in real music), the EMA-based
// normalization chain (mImm → mAvg → mLongAvg) converges, so
// mImm / mLongAvg ≈ 1.0 at all times. Bass works because it has impulsive
// content (spikes followed by silence), creating divergence between
// immediate and long-term averages.

#include "test.h"

FL_TEST_FILE(FL_FILEPATH) {
#include "tests/misc/vibe_mid_treb_dynamic_range.hpp"
} // FL_TEST_FILE
