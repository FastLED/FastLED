// Early-boot loop guard: the bench recovers by itself when a firmware hangs
// or crash-loops, including before USB comes up. Each boot that never reaches
// loop() counts toward the escape; the third drops into the bootloader so the
// next deploy can flash the board. The platform owns the details (see
// fl/wdt/boot_guard.h); where it has no early-boot hook this is a no-op.
//
// The guard lives in this .cpp, not the .ino: Arduino prototype generation can
// give .ino startup hooks C++ linkage. setup() ends with markBootHealthy().
//
// Hardware test: build with -DFL_WATCHDOG_DEBUG_EARLY_HANG and the board hangs
// before USB on every boot; it must come back in its bootloader by itself.

#include "FastLED.h"
#include "fl/wdt/boot_guard.h"

#ifndef AUTORESEARCH_BOOT_GUARD_ESCAPE_BOOTS
#define AUTORESEARCH_BOOT_GUARD_ESCAPE_BOOTS 3
#endif

// Covers USB bring-up, static constructors and setup(), which re-arms the
// watchdog with its own timeout.
#ifndef AUTORESEARCH_BOOT_GUARD_TIMEOUT_MS
#define AUTORESEARCH_BOOT_GUARD_TIMEOUT_MS 30000
#endif

FL_WATCHDOG_BOOT_GUARD(AUTORESEARCH_BOOT_GUARD_ESCAPE_BOOTS,
                       AUTORESEARCH_BOOT_GUARD_TIMEOUT_MS);
