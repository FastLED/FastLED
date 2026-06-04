// IWYU pragma: private

/// @file platforms/arm/samd/watchdog_samd.impl.hpp
/// @brief SAMD21 / SAMD51 watchdog implementation via Adafruit SleepyDog.
///
/// Uses the well-tested `Adafruit_SleepyDog` library which handles the SAMD
/// WDT register sync-busy waits, GCLK setup, and timeout rounding
/// (SAMD WDT only supports power-of-two timeouts in the 8 ms .. 16 s range).
///
/// Reset cause from `PM->RCAUSE` (SAMD21) / `RSTC->RCAUSE` (SAMD51) is
/// translated by the SleepyDog `Watchdog.resetCause()` extension where
/// available; otherwise we read the register directly.
///
/// Persist storage is in-RAM; Phase 3 follow-up can move it to SAMD51
/// SmartEEPROM or to FlashStorage for SAMD21.

#include "fl/wdt/watchdog.h"

// IWYU pragma: begin_keep
#include <Adafruit_SleepyDog.h>   // ok include — Adafruit watchdog wrapper
// IWYU pragma: end_keep

#define FL_WATCHDOG_HAS_HARDWARE
#define FL_WATCHDOG_PERSIST_BYTES 8
#define FL_WATCHDOG_MAX_TIMEOUT_MS 16000u

namespace fl {
namespace platforms {

struct SamdWatchdogState {
    fl::u8     persist[FL_WATCHDOG_PERSIST_BYTES] = {0};
    fl::u16    crash_count = 0;
    bool       armed = false;
    ResetCause cached_cause = ResetCause::UNKNOWN;
    bool       cause_cached = false;
};

inline SamdWatchdogState& samdWatchdogState() {
    static SamdWatchdogState s{};  // okay static in header — single-TU `.impl.hpp`
    return s;
}

} // namespace platforms

Watchdog& Watchdog::instance() FL_NOEXCEPT {
    static Watchdog sInstance;
    return sInstance;
}

void Watchdog::begin(fl::u32 timeout_ms) FL_NOEXCEPT {
    if (timeout_ms == 0) timeout_ms = 1000;
    if (timeout_ms > FL_WATCHDOG_MAX_TIMEOUT_MS) timeout_ms = FL_WATCHDOG_MAX_TIMEOUT_MS;
    ::Watchdog.enable(static_cast<int>(timeout_ms));
    platforms::samdWatchdogState().armed = true;
}

void Watchdog::feed() FL_NOEXCEPT {
    ::Watchdog.reset();
}

void Watchdog::disable() FL_NOEXCEPT {
    ::Watchdog.disable();
    platforms::samdWatchdogState().armed = false;
}

ResetCause Watchdog::lastResetCause() const FL_NOEXCEPT {
    auto& s = platforms::samdWatchdogState();
    if (!s.cause_cached) {
        // Adafruit_SleepyDog's resetCause(): 0=POR, 1=BOD12, 2=BOD33,
        // 4=external pin, 5=WDT, 6=system reset request, 7=backup.
        int rc = ::Watchdog.resetCause();
        switch (rc) {
            case 5:  s.cached_cause = ResetCause::WATCHDOG; break;
            case 6:  s.cached_cause = ResetCause::SOFTWARE; break;
            case 4:  s.cached_cause = ResetCause::EXTERNAL_PIN; break;
            case 1:
            case 2:  s.cached_cause = ResetCause::BROWNOUT; break;
            case 0:  s.cached_cause = ResetCause::POWER_ON; break;
            default: s.cached_cause = ResetCause::UNKNOWN; break;
        }
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
    return platforms::samdWatchdogState().persist[idx];
}
void Watchdog::persistWrite(fl::size idx, fl::u8 v) FL_NOEXCEPT {
    if (idx >= FL_WATCHDOG_PERSIST_BYTES) return;
    platforms::samdWatchdogState().persist[idx] = v;
}

fl::u16 Watchdog::consecutiveCrashCount() const FL_NOEXCEPT {
    (void)lastResetCause();
    return platforms::samdWatchdogState().crash_count;
}
void Watchdog::markCleanShutdown() FL_NOEXCEPT { platforms::samdWatchdogState().crash_count = 0; }
bool Watchdog::isInSafeMode() const FL_NOEXCEPT { return consecutiveCrashCount() >= mSafeModeThreshold; }
fl::u16 Watchdog::safeModeThreshold() const FL_NOEXCEPT { return mSafeModeThreshold; }
void    Watchdog::setSafeModeThreshold(fl::u16 t) FL_NOEXCEPT { mSafeModeThreshold = t; }

FL_NORETURN void Watchdog::reboot() FL_NOEXCEPT {
    NVIC_SystemReset();
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
    WatchdogCrashReport r{}; r.valid = false; r.fault_type = ""; return r;
}
void Watchdog::clearCrashReport() FL_NOEXCEPT {}

} // namespace fl
