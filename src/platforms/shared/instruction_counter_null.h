#pragma once

// IWYU pragma: private

#include "fl/stl/int.h"
#include "fl/stl/noexcept.h"

namespace fl {
namespace platforms {

/// No instruction counter on this target: start() and stop() do nothing and
/// stop() reports 0. Check InstructionCounter::kSupported before reporting.
class InstructionCounter {
  public:
    static constexpr bool kSupported = false;
    static u32 readCycles() FL_NO_EXCEPT { return 0; }
    void start() FL_NO_EXCEPT {}
    u32 stop(u32 elapsed_cycles) FL_NO_EXCEPT {
        (void)elapsed_cycles;
        return 0;
    }
};

}  // namespace platforms
}  // namespace fl
