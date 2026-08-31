#pragma once

// IWYU pragma: private

#include "fl/stl/chrono.h"
#include "fl/stl/noexcept.h"

namespace fl {
namespace platforms {

using StubWatchdogClock = fl::chrono::steady_clock::time_point (*)();

void setStubWatchdogClockForTesting(StubWatchdogClock clock) FL_NO_EXCEPT;
void clearStubWatchdogClockForTesting() FL_NO_EXCEPT;

} // namespace platforms
} // namespace fl
