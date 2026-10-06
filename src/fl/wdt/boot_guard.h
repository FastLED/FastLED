#pragma once

/// @file fl/wdt/boot_guard.h
/// @brief Early-boot loop guard: escape to the bootloader when firmware keeps
/// failing before it ever reaches a healthy state, even before USB is up.
///
/// The watchdog's crash-count escape (`Watchdog::escapeToBootloaderIfLooping`,
/// #3713) runs from `setup()`. A firmware that hangs or faults before that
/// point -- during static construction, or before the USB stack enumerates --
/// never gets there, and the host cannot reach the board. The boot guard runs
/// in the platform's earliest startup hook instead:
///
///   1. every boot increments a counter kept in reset-persistent memory;
///   2. the hardware watchdog is armed with `early_timeout_ms`, so a hang
///      becomes a reset instead of a permanent wedge;
///   3. once the counter reaches `escape_boots`, the hook zeroes it and
///      reboots into the bootloader, where the host can flash the board;
///   4. the sketch calls `FastLED.watchdog().markBootHealthy()` once it is
///      healthy (e.g. at the end of `setup()`), which zeroes the counter.
///
/// The early watchdog stays armed: the sketch must take it over with
/// `FastLED.watchdog().begin()`/`feed()`, or call `disable()`.
///
/// Opt in by placing the macro once, at file scope, in a sketch `.cpp` file
/// (not a `.ino`: Arduino prototype generation can give C-linkage startup
/// hooks the wrong linkage):
/// @code
///   #include "FastLED.h"
///   FL_WATCHDOG_BOOT_GUARD(3, 30000);
/// @endcode
///
/// Platforms that support it define `FL_WATCHDOG_HAS_BOOT_GUARD` (Teensy 4.x
/// today). Everywhere else the macro expands to nothing,
/// `bootGuardCount()` returns 0 and `markBootHealthy()` does nothing.
///
/// **Hardware test option:** build with `-DFL_WATCHDOG_DEBUG_EARLY_HANG` to
/// make the guard hang forever right after arming, every boot. A supported
/// board must then come back in its bootloader by itself. Never ship it.

#include "fl/stl/static_assert.h"
#include "fl/wdt/boot_guard_record.h"  // IWYU pragma: export
// The platform header defines FL_WATCHDOG_BOOT_GUARD_IMPL when it supports
// the early-boot guard.
#include "platforms/watchdog_boot_guard.h"  // IWYU pragma: export

#ifdef FL_WATCHDOG_BOOT_GUARD_IMPL
/// @brief Install the early-boot loop guard. Use once, at file scope, in a
/// sketch `.cpp`. See the file comment.
#define FL_WATCHDOG_BOOT_GUARD(escape_boots, early_timeout_ms) \
    FL_WATCHDOG_BOOT_GUARD_IMPL(escape_boots, early_timeout_ms)
#else
// Documented no-op where the platform has no early-boot hook.
#define FL_WATCHDOG_BOOT_GUARD(escape_boots, early_timeout_ms) \
    FL_STATIC_ASSERT((escape_boots) >= 0 && (early_timeout_ms) >= 0, "")
#endif
