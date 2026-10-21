#pragma once

// IWYU pragma: private

#include "fl/stl/chrono.h"
#include "fl/stl/noexcept.h"
#include "fl/stl/stdint.h"

namespace fl {
namespace platforms {

using StubWatchdogClock = fl::chrono::steady_clock::time_point (*)();

void setStubWatchdogClockForTesting(StubWatchdogClock clock) FL_NO_EXCEPT;
void clearStubWatchdogClockForTesting() FL_NO_EXCEPT;

/// Boot-guard simulation. The image key stands in for the firmware image:
/// changing it simulates a reflash.
void setStubBootGuardImageForTesting(fl::u32 image) FL_NO_EXCEPT;
/// Run the early-boot hook's logic (FL_WATCHDOG_BOOT_GUARD): count the boot
/// and arm the watchdog with `early_timeout_ms`. Returns true where real
/// hardware would reboot into the bootloader (the timer is then not armed).
bool stubBootGuardEarlyBootForTesting(fl::u32 escape_boots,
                                      fl::u32 early_timeout_ms) FL_NO_EXCEPT;
/// True while the stub watchdog timer is running.
bool stubWatchdogEnabledForTesting() FL_NO_EXCEPT;

} // namespace platforms
} // namespace fl
