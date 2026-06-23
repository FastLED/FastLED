// IWYU pragma: private

/// @file fl/wdt/watchdog.cpp.hpp
/// @brief Platform-agnostic implementations for the unified watchdog API:
///   - ResetInfo::describe()        Tier 1 human-readable description writer
///   - ResetInfo::subcauseName()    Tier 1 default (per-platform impls may shadow)
///   - Watchdog::lastResetInfo()    Tier 1 default (returns {cause, 0, 0})
///   - ScopedWatchdog ctor/dtor     Tier 1.5 RAII guard
///
/// Per FastLED conventions this file is included from exactly one translation
/// unit (the platform dispatcher includes it after the per-platform
/// `.impl.hpp`), and is marked `// IWYU pragma: private`.

#include "fl/wdt/watchdog.h"

namespace fl {

// =============================================================================
// ResetInfo helpers
// =============================================================================

// Default subcauseName(): empty view. Per-platform richer subcause naming
// will be wired through a follow-up issue (#2755 tracks the design); for
// now the empty default works for every backend and describe() handles
// the subcauseId==0 case by skipping the parenthesized name.
fl::string_view ResetInfo::subcauseName() const FL_NOEXCEPT {
    return fl::string_view();
}

namespace platforms {

// Write `value` as a lowercase hex string into `out`, prefixed with "0x".
// Returns bytes written (always <= 10, since u32 max is 0xFFFFFFFF = 8 hex
// digits + 2 prefix chars). Does not write a NUL. Truncates if out is small.
inline fl::size writeHex(fl::span<char> out, fl::u32 value) FL_NOEXCEPT {
    char tmp[10];
    tmp[0] = '0';
    tmp[1] = 'x';
    static const char hex[] = "0123456789abcdef";
    for (int i = 7; i >= 0; --i) {
        tmp[2 + (7 - i)] = hex[(value >> (i * 4)) & 0xFu];
    }
    fl::size n = (out.size() < 10) ? out.size() : 10;
    for (fl::size i = 0; i < n; ++i) out[i] = tmp[i];
    return n;
}

// Copy a string_view into out[]. Returns bytes copied, truncated to out.size().
inline fl::size writeView(fl::span<char> out, fl::string_view sv) FL_NOEXCEPT {
    fl::size n = (out.size() < sv.size()) ? out.size() : sv.size();
    for (fl::size i = 0; i < n; ++i) out[i] = sv[i];
    return n;
}

inline fl::size writeChar(fl::span<char> out, char c) FL_NOEXCEPT {
    if (out.size() == 0) return 0;
    out[0] = c;
    return 1;
}

inline fl::size writeNulIfRoom(fl::span<char> out, fl::size written) FL_NOEXCEPT {
    if (written < out.size()) out[written] = '\0';
    return written;
}

} // namespace platforms

fl::size ResetInfo::describe(fl::span<char> out, bool verbose) const FL_NOEXCEPT {
    fl::size n = 0;
    // <cause>
    n += platforms::writeView(out.subspan(n), causeName());
    // ( <sub> )  if subcauseId != 0
    if (subcauseId != 0) {
        fl::string_view sub = subcauseName();
        if (sub.size() != 0) {
            n += platforms::writeView(out.subspan(n), fl::string_view(" ("));
            n += platforms::writeView(out.subspan(n), sub);
            n += platforms::writeChar(out.subspan(n), ')');
        }
    }
    if (verbose) {
        n += platforms::writeView(out.subspan(n), fl::string_view(" raw="));
        n += platforms::writeHex(out.subspan(n), rawRegister);
    }
    return platforms::writeNulIfRoom(out, n);
}

// =============================================================================
// Watchdog::lastResetInfo() default — platforms can override
// =============================================================================

// Default: lift the normalized cause into a ResetInfo, leaving
// subcauseId/rawRegister at 0. Per-platform richer extraction will be wired
// through a follow-up to issue #2755 (every backend uses this default for now).
ResetInfo Watchdog::lastResetInfo() const FL_NOEXCEPT {
    ResetInfo info{};
    info.cause = lastResetCause();
    info.subcauseId = 0;
    info.rawRegister = 0;
    return info;
}

// =============================================================================
// ScopedWatchdog
// =============================================================================

namespace platforms {

// Forward-declared in this TU to avoid pulling in fl::print/println headers
// from a public header. The dispatcher TU includes the necessary print
// header before this file.
void scopedWatchdogPrintLine(fl::string_view sv) FL_NOEXCEPT;
void scopedWatchdogPause3s() FL_NOEXCEPT;

// Counts simultaneously-alive ScopedWatchdog instances. >1 is almost always
// a programmer error: nested guards mean the lazy-init's timeout argument
// of the second instance is silently ignored, and timeout semantics across
// the two scopes become ambiguous. Singleton-style accessor (function-local
// static) so the counter has well-defined lifetime even if a guard is
// constructed before main().
inline int& scopedWatchdogActiveCount() FL_NOEXCEPT {
    static int count = 0;
    return count;
}

// First-init helper, used by ScopedWatchdog's constructor. Runs only once
// per process. Single-threaded loop() is the canonical caller — no atomics
// are needed for the guard byte on supported MCUs.
inline void scopedWatchdogFirstInit(fl::u32 timeout_ms) FL_NOEXCEPT {
    Watchdog& wdt = Watchdog::instance();
    wdt.begin(timeout_ms);

    ResetInfo info = wdt.lastResetInfo();
    if (info.cause == ResetCause::WATCHDOG ||
        info.cause == ResetCause::PANIC    ||
        info.cause == ResetCause::LOCKUP) {
        char buf[128];
        fl::size n = info.describe(fl::span<char>(buf, sizeof(buf)), /*verbose=*/true);
        scopedWatchdogPrintLine(fl::string_view("[FastLED.watchdog] recovered from:"));
        scopedWatchdogPrintLine(fl::string_view(buf, n));

        if (wdt.hasCrashReport()) {
            // Hook for Tier 2 platforms. Future revision: also emit a
            // multi-line CrashReport::describe() result here. For now the
            // bare cause + raw register is the documented contract.
        }
        scopedWatchdogPause3s();
    }
    scopedWatchdogPrintLine(fl::string_view("[FastLED.watchdog] armed via FL_WATCHDOG_AUTO()"));
}

} // namespace platforms

ScopedWatchdog::ScopedWatchdog(fl::u32 timeout_ms) FL_NOEXCEPT {
    static bool sInitialized = false;
    if (!sInitialized) {
        sInitialized = true;
        platforms::scopedWatchdogFirstInit(timeout_ms);
    }

    // Single-instance enforcement. A second simultaneously-alive ScopedWatchdog
    // is almost always a bug — the timeout argument here was silently ignored
    // (only the first instance gets to arm the WDT). Warn once per process so
    // the developer notices, but still feed so the program survives.
    int& count = platforms::scopedWatchdogActiveCount();
    if (count >= 1) {
        static bool sWarnedOnce = false;
        if (!sWarnedOnce) {
            sWarnedOnce = true;
            platforms::scopedWatchdogPrintLine(fl::string_view(
                "[FastLED.watchdog] WARN: nested FL_WATCHDOG_AUTO() detected — "
                "only one scoped guard should be alive at a time"));
        }
    }
    ++count;

    Watchdog::instance().feed();
}

ScopedWatchdog::~ScopedWatchdog() FL_NOEXCEPT {
    int& count = platforms::scopedWatchdogActiveCount();
    if (count > 0) --count;
    Watchdog::instance().feed();
}

int ScopedWatchdog::activeScopeCount() FL_NOEXCEPT {
    return platforms::scopedWatchdogActiveCount();
}

} // namespace fl
