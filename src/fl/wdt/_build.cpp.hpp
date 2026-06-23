/// @file _build.cpp.hpp
/// @brief Unity build header for fl/wdt/ directory.
///
/// Includes the platform-agnostic watchdog API helpers (ResetInfo::describe,
/// ScopedWatchdog ctor/dtor, etc.) plus the per-TU print/delay sink that
/// ScopedWatchdog's lazy first-init relies on.

#include "fl/log/log.h"
#include "fl/system/delay.h"

#include "fl/wdt/watchdog.cpp.hpp"

namespace fl {
namespace platforms {

// Print-sink implementation used by ScopedWatchdog's first-init path. Defined
// here (in the unity TU) rather than in watchdog.cpp.hpp so that public
// headers don't pull in fl/log/log.h.
void scopedWatchdogPrintLine(fl::string_view sv) FL_NOEXCEPT {
    FL_WARN(sv);
}

// Wraps fl::delay() — portable across stub/WASM/embedded.
void scopedWatchdogPause3s() FL_NOEXCEPT {
    fl::delay(3000);
}

} // namespace platforms
} // namespace fl
