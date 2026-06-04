// IWYU pragma: private

/// @file platforms/arm/mxrt1062/watchdog_mxrt1062.impl.hpp
/// @brief Teensy 4.x (iMXRT1062) watchdog implementation.
///
/// Uses `WDT_T4<WDT3>` (RTWDOG) from the bundled Teensyduino `Watchdog_t4.h`
/// library. **WDOG1/WDOG2 are intentionally NOT used** — they're configured
/// by the ROM/bootloader and resetting through them can interact with the
/// MKL02 bootloader chip in ways the previous attempt (see
/// `AutoResearch.ino:204-214`) discovered the hard way.
///
/// Reset cause from `SRC_SRSR`. Persist storage is in-RAM only for now;
/// Phase 3 follow-up will move it to OCRAM2 retained region (above
/// `CrashReport` at `0x2027FF80`) for soft-reset survival.

#include "fl/wdt/watchdog.h"

// IWYU pragma: begin_keep
#include <Watchdog_t4.h>   // ok include — bundled Teensyduino library
#include <imxrt.h>         // ok include — SRC_SRSR register
// IWYU pragma: end_keep

#define FL_WATCHDOG_HAS_HARDWARE
#define FL_WATCHDOG_PERSIST_BYTES 16
#define FL_WATCHDOG_MAX_TIMEOUT_MS 128000u
// NOTE: WDT_T4 / RTWDOG technically exposes pre-timeout IRQ + windowed mode,
// but our Tier-1/2 wrappers (`onTimeout()`, `setWindow()`) currently return
// false on this platform. Capability macros must match runtime behavior, so
// we deliberately do NOT advertise `FL_WATCHDOG_HAS_PRE_TIMEOUT_IRQ` /
// `FL_WATCHDOG_HAS_WINDOW_MODE` until those wrappers are implemented in a
// follow-up.

namespace fl {
namespace platforms {

struct Mxrt1062WatchdogState {
    fl::u8     persist[FL_WATCHDOG_PERSIST_BYTES] = {0};
    fl::u16    crash_count = 0;
    bool       armed = false;
    fl::u32    armed_timeout_ms = 0;
    ResetCause cached_cause = ResetCause::UNKNOWN;
    bool       cause_cached = false;
};

inline Mxrt1062WatchdogState& mxrt1062WatchdogState() {
    static Mxrt1062WatchdogState s{};  // okay static in header — single-TU `.impl.hpp`
    return s;
}

inline WDT_T4<WDT3>& mxrt1062Wdt() {
    static WDT_T4<WDT3> w;  // okay static in header — single-TU `.impl.hpp`
    return w;
}

inline ResetCause translateSrcSrsr(fl::u32 srsr) {
    // Bits per iMXRT1062 reference manual. Priority: most-specific first.
    if (srsr & (1u << 16)) return ResetCause::WATCHDOG;      // WDOG3_RST_B
    if (srsr & (1u << 7))  return ResetCause::WATCHDOG;      // WDOG_RST_B (WDOG1/2)
    if (srsr & (1u << 4))  return ResetCause::LOCKUP;        // LOCKUP_SYSRESETREQ
    if (srsr & (1u << 8))  return ResetCause::DEBUGGER;      // JTAG_SW_RST
    if (srsr & (1u << 5))  return ResetCause::EXTERNAL_PIN;  // IPP_USER_RESET_B
    if (srsr & (1u << 0))  return ResetCause::POWER_ON;      // POR
    return ResetCause::UNKNOWN;
}

} // namespace platforms

Watchdog& Watchdog::instance() FL_NOEXCEPT {
    static Watchdog sInstance;
    return sInstance;
}

void Watchdog::begin(fl::u32 timeout_ms) FL_NOEXCEPT {
    // WDT_T4 minimum is 500 ms; clamp.
    if (timeout_ms < 500) timeout_ms = 500;
    if (timeout_ms > FL_WATCHDOG_MAX_TIMEOUT_MS) timeout_ms = FL_WATCHDOG_MAX_TIMEOUT_MS;
    auto& s = platforms::mxrt1062WatchdogState();
    WDT_timings_t cfg;
    cfg.timeout = static_cast<float>(timeout_ms) / 1000.0f;
    platforms::mxrt1062Wdt().begin(cfg);
    s.armed = true;
    s.armed_timeout_ms = timeout_ms;
}

void Watchdog::feed() FL_NOEXCEPT {
    platforms::mxrt1062Wdt().feed();
}

void Watchdog::disable() FL_NOEXCEPT {
    // WDT_T4 / RTWDOG cannot be cleanly disabled once armed (CS write-protect).
    // The honest action is to keep feeding it from a background context, but
    // we have nowhere to do that from a Tier-0 disable(). Document the
    // limitation: post-disable() the watchdog still ticks. Subsequent
    // feed() calls still work; if the caller wanted to stop monitoring it
    // should also stop calling feed() — but in that case the WDT will fire.
    // We do NOT clear armed because the watchdog is in fact still armed.
}

ResetCause Watchdog::lastResetCause() const FL_NOEXCEPT {
    auto& s = platforms::mxrt1062WatchdogState();
    if (!s.cause_cached) {
        s.cached_cause = platforms::translateSrcSrsr(SRC_SRSR);
        s.cause_cached = true;
        if (s.cached_cause == ResetCause::WATCHDOG && s.crash_count < 0xFFFF) {
            s.crash_count++;
        }
    }
    return s.cached_cause;
}

bool Watchdog::lastResetWasWatchdog() const FL_NOEXCEPT {
    return lastResetCause() == ResetCause::WATCHDOG;
}

fl::u8 Watchdog::persistRead(fl::size idx) const FL_NOEXCEPT {
    if (idx >= FL_WATCHDOG_PERSIST_BYTES) return 0;
    return platforms::mxrt1062WatchdogState().persist[idx];
}

void Watchdog::persistWrite(fl::size idx, fl::u8 v) FL_NOEXCEPT {
    if (idx >= FL_WATCHDOG_PERSIST_BYTES) return;
    platforms::mxrt1062WatchdogState().persist[idx] = v;
}

fl::u16 Watchdog::consecutiveCrashCount() const FL_NOEXCEPT {
    (void)lastResetCause();
    return platforms::mxrt1062WatchdogState().crash_count;
}

void Watchdog::markCleanShutdown() FL_NOEXCEPT {
    platforms::mxrt1062WatchdogState().crash_count = 0;
}

bool Watchdog::isInSafeMode() const FL_NOEXCEPT {
    return consecutiveCrashCount() >= mSafeModeThreshold;
}

fl::u16 Watchdog::safeModeThreshold() const FL_NOEXCEPT { return mSafeModeThreshold; }
void    Watchdog::setSafeModeThreshold(fl::u16 t) FL_NOEXCEPT { mSafeModeThreshold = t; }

FL_NORETURN void Watchdog::reboot() FL_NOEXCEPT {
    SCB_AIRCR = 0x05FA0004;  // SYSRESETREQ with VECTKEY
    while (true) {}
}

bool Watchdog::onTimeout(WatchdogTimeoutCallback, void*) FL_NOEXCEPT { return false; }
bool Watchdog::onTimeout(fl::function<void()>) FL_NOEXCEPT { return false; }
bool Watchdog::setPauseOnDebug(bool) FL_NOEXCEPT { return false; }
bool Watchdog::writeCrashLog(fl::span<const fl::u8>) FL_NOEXCEPT { return false; }
fl::size Watchdog::readCrashLog(fl::span<fl::u8>) const FL_NOEXCEPT { return 0; }
bool Watchdog::rebootIntoBootloader() FL_NOEXCEPT { return false; }

bool Watchdog::setWindow(fl::u32, fl::u32) FL_NOEXCEPT { return false; }
bool Watchdog::hasCrashReport() const FL_NOEXCEPT { return false; }
WatchdogCrashReport Watchdog::readCrashReport() const FL_NOEXCEPT {
    WatchdogCrashReport r{};
    r.valid = false;
    r.fault_type = "";
    return r;
}
void Watchdog::clearCrashReport() FL_NOEXCEPT {}

} // namespace fl
