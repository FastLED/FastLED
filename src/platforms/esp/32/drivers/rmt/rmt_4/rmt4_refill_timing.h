#pragma once

// IWYU pragma: private

#include "fl/stl/atomic.h"
#include "fl/stl/compiler_control.h"
#include "fl/stl/noexcept.h"

namespace fl {
namespace detail {

/// Internal translator deadline state; reset before each SDK transmission.
class Rmt4RefillTiming {
  public:
    void reset() FL_NO_EXCEPT;
    bool allowFill(u32 now, u32 maxCyclesPerFill) FL_NO_EXCEPT;

  private:
    fl::atomic_u32 mLastFill;
    fl::atomic_u32 mFillPhase;
};

} // namespace detail
} // namespace fl
