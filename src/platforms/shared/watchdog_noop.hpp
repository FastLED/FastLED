// IWYU pragma: private

/// @file platforms/shared/watchdog_noop.hpp
/// @brief No-op watchdog fallback for platforms without a real or emulated WDT.
///
/// Follows the FastLED `_noop.hpp` convention (see agents/docs/cpp-standards.md
/// "No-op Fallback"): inline functions, `fl::platforms::` namespace, bodies
/// return inert defaults — never assert, throw, or log.
///
/// This file is included by `src/platforms/watchdog.impl.cpp.hpp` in the
/// dispatcher's `#else` branch. It also provides default zero/false values for
/// every `FL_WATCHDOG_*` numeric capability macro so user code that reads
/// them compiles unconditionally.

#include "fl/wdt/watchdog.h"

// ----------------------------------------------------------------------------
// Capability macros — defaults for the noop platform.
// Boolean flags are NOT defined (so #ifdef FL_WATCHDOG_HAS_* is false).
// Numerics get a sensible zero/default value.
// ----------------------------------------------------------------------------

#ifndef FL_WATCHDOG_PERSIST_BYTES
#define FL_WATCHDOG_PERSIST_BYTES 0
#endif

#ifndef FL_WATCHDOG_MAX_TIMEOUT_MS
#define FL_WATCHDOG_MAX_TIMEOUT_MS 0
#endif

namespace fl {

// Non-inline definitions — this header is `// IWYU pragma: private` and only
// included from `platforms/watchdog.impl.cpp.hpp` in the dispatcher's TU, so
// each method is defined exactly once across the program.
//
// `Watchdog::instance()` is also defined here (and in every sibling
// `.impl.hpp`) so the function-local static lives in exactly one TU per
// program — keeps Teensy 3.x clear of the `__cxa_guard` ABI conflict.

Watchdog& Watchdog::instance() FL_NOEXCEPT {
    static Watchdog sInstance;
    return sInstance;
}

// ---- Tier 0 ----

void       Watchdog::begin(fl::u32 /*timeout_ms*/) FL_NOEXCEPT {}
void       Watchdog::feed() FL_NOEXCEPT {}
void       Watchdog::disable() FL_NOEXCEPT {}

ResetCause Watchdog::lastResetCause() const FL_NOEXCEPT { return ResetCause::UNKNOWN; }
bool       Watchdog::lastResetWasWatchdog() const FL_NOEXCEPT { return false; }

fl::u8     Watchdog::persistRead(fl::size /*idx*/) const FL_NOEXCEPT { return 0; }
void       Watchdog::persistWrite(fl::size /*idx*/, fl::u8 /*v*/) FL_NOEXCEPT {}

fl::u16    Watchdog::consecutiveCrashCount() const FL_NOEXCEPT { return 0; }
void       Watchdog::markCleanShutdown() FL_NOEXCEPT {}
bool       Watchdog::isInSafeMode() const FL_NOEXCEPT { return false; }
fl::u16    Watchdog::safeModeThreshold() const FL_NOEXCEPT { return mSafeModeThreshold; }
void       Watchdog::setSafeModeThreshold(fl::u16 t) FL_NOEXCEPT { mSafeModeThreshold = t; }

// reboot() is FL_NORETURN — even on noop, we must spin forever so the
// signature contract holds. Using a loop instead of abort() so we don't
// require <cstdlib> from a header used on bare-metal AVR.
FL_NORETURN void Watchdog::reboot() FL_NOEXCEPT {
    while (true) {}
}

// ---- Tier 1 ----

bool       Watchdog::onTimeout(WatchdogTimeoutCallback /*cb*/, void* /*ud*/) FL_NOEXCEPT { return false; }
bool       Watchdog::onTimeout(fl::function<void()> /*cb*/) FL_NOEXCEPT { return false; }
bool       Watchdog::setPauseOnDebug(bool /*pause*/) FL_NOEXCEPT { return false; }
bool       Watchdog::writeCrashLog(fl::span<const fl::u8> /*payload*/) FL_NOEXCEPT { return false; }
fl::size   Watchdog::readCrashLog(fl::span<fl::u8> /*out*/) const FL_NOEXCEPT { return 0; }
bool       Watchdog::rebootIntoBootloader() FL_NOEXCEPT { return false; }

// ---- Tier 2 ----

bool                Watchdog::setWindow(fl::u32, fl::u32) FL_NOEXCEPT { return false; }
bool                Watchdog::hasCrashReport() const FL_NOEXCEPT { return false; }
WatchdogCrashReport Watchdog::readCrashReport() const FL_NOEXCEPT {
    WatchdogCrashReport r{};
    r.valid = false;
    r.fault_type = "";
    return r;
}
void                Watchdog::clearCrashReport() FL_NOEXCEPT {}

} // namespace fl
