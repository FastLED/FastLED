// IWYU pragma: private

/// @file platforms/shared/watchdog_boot_guard_noop.hpp
/// @brief No-op boot-guard counter storage (fl/wdt/boot_guard.h) for
/// platforms without reset-persistent storage for it. Included only from
/// `platforms/watchdog.impl.cpp.hpp`.

#include "fl/wdt/watchdog.h"

namespace fl {
namespace platforms {

fl::u32 watchdogBootGuardRead() FL_NO_EXCEPT { return 0; }
void    watchdogBootGuardWrite(fl::u32 /*boots*/) FL_NO_EXCEPT {}
void    watchdogBootGuardReleaseEarlyTimer() FL_NO_EXCEPT {}

} // namespace platforms
} // namespace fl
