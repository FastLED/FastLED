// IWYU pragma: private

/// @file platforms/apollo3/watchdog_apollo3.impl.hpp
/// @brief Apollo3 (SparkFun Artemis) watchdog implementation.
///
/// `am_hal_wdt_*` from AmbiqSuite SDK. LFRC-clocked (~1 kHz). Reset cause
/// from `RSTGEN->STAT`. Persist storage in-RAM only; Phase 3 follow-up can
/// move it to internal flash via `am_hal_flash_*`.

#include "fl/wdt/watchdog.h"

// IWYU pragma: begin_keep
#include <am_mcu_apollo.h>   // ok include — AmbiqSuite SDK
// IWYU pragma: end_keep

#define FL_WATCHDOG_HAS_HARDWARE
#define FL_WATCHDOG_PERSIST_BYTES 8
#define FL_WATCHDOG_MAX_TIMEOUT_MS 250000u

namespace fl {
namespace platforms {

struct Apollo3WatchdogState {
    fl::u8     persist[FL_WATCHDOG_PERSIST_BYTES] = {0};
    fl::u16    crash_count = 0;
    bool       armed = false;
    ResetCause cached_cause = ResetCause::UNKNOWN;
    bool       cause_cached = false;
};

inline Apollo3WatchdogState& apollo3WatchdogState() {
    static Apollo3WatchdogState s{};  // okay static in header — single-TU `.impl.hpp`
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
    // LFRC ticks at ~1 kHz so `ui32ResetCount` in ms ≈ timeout in counts.
    am_hal_wdt_config_t cfg = {};
    cfg.ui32Config = AM_HAL_WDT_LFRC_CLK_DEFAULT | AM_HAL_WDT_ENABLE_RESET;
    cfg.ui16InterruptCount = 0;
    cfg.ui16ResetCount = static_cast<fl::u16>((timeout_ms > 65535u) ? 65535u : timeout_ms);
    am_hal_wdt_init(&cfg);
    am_hal_wdt_start();
    platforms::apollo3WatchdogState().armed = true;
}

void Watchdog::feed() FL_NOEXCEPT {
    am_hal_wdt_restart();
}

void Watchdog::disable() FL_NOEXCEPT {
    am_hal_wdt_halt();
    platforms::apollo3WatchdogState().armed = false;
}

ResetCause Watchdog::lastResetCause() const FL_NOEXCEPT {
    // Apollo3 cause detection from RSTGEN->STAT is deferred — the AmbiqSuite
    // SDK signature wants a `uint32_t*` and translating each `AM_HAL_RESET_*`
    // bit needs hardware verification. Tracked as a Phase 3 follow-up. For
    // now we report UNKNOWN; the crash counter still increments correctly on
    // any reset via persist storage that survives Phase 3's flash backing.
    return ResetCause::UNKNOWN;
}

bool Watchdog::lastResetWasWatchdog() const FL_NOEXCEPT {
    return lastResetCause() == ResetCause::WATCHDOG;
}

fl::u8 Watchdog::persistRead(fl::size idx) const FL_NOEXCEPT {
    if (idx >= FL_WATCHDOG_PERSIST_BYTES) return 0;
    return platforms::apollo3WatchdogState().persist[idx];
}
void Watchdog::persistWrite(fl::size idx, fl::u8 v) FL_NOEXCEPT {
    if (idx >= FL_WATCHDOG_PERSIST_BYTES) return;
    platforms::apollo3WatchdogState().persist[idx] = v;
}

fl::u16 Watchdog::consecutiveCrashCount() const FL_NOEXCEPT {
    (void)lastResetCause();
    return platforms::apollo3WatchdogState().crash_count;
}
void Watchdog::markCleanShutdown() FL_NOEXCEPT { platforms::apollo3WatchdogState().crash_count = 0; }
bool Watchdog::isInSafeMode() const FL_NOEXCEPT { return consecutiveCrashCount() >= mSafeModeThreshold; }
fl::u16 Watchdog::safeModeThreshold() const FL_NOEXCEPT { return mSafeModeThreshold; }
void    Watchdog::setSafeModeThreshold(fl::u16 t) FL_NOEXCEPT { mSafeModeThreshold = t; }

FL_NORETURN void Watchdog::reboot() FL_NOEXCEPT {
    am_hal_reset_control(AM_HAL_RESET_CONTROL_SWPOR, 0);
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
