// IWYU pragma: private

/// @file platforms/arm/mgm240/watchdog_mgm240.impl.hpp
/// @brief MGM240 (Silicon Labs EFR32MG24, Arduino Nano Matter) watchdog impl.
///
/// `WDOG0` via `em_wdog.h` from the Gecko SDK. **Avoid `WDOG1`** — it may be
/// claimed by the Matter / OpenThread stack on radio-enabled builds.
///
/// Reset cause from `EMU->RSTCAUSE`. Persist storage in-RAM only; Phase 3
/// follow-up can move it to BURAM / RETREG / NVM3.

#include "fl/wdt/watchdog.h"

// IWYU pragma: begin_keep
#include <em_wdog.h>   // ok include — SiLabs Gecko SDK
#include <em_emu.h>    // ok include — EMU->RSTCAUSE
// IWYU pragma: end_keep

#define FL_WATCHDOG_HAS_HARDWARE
#define FL_WATCHDOG_PERSIST_BYTES 8
#define FL_WATCHDOG_MAX_TIMEOUT_MS 32000u

namespace fl {
namespace platforms {

struct Mgm240WatchdogState {
    fl::u8     persist[FL_WATCHDOG_PERSIST_BYTES] = {0};
    fl::u16    crash_count = 0;
    bool       armed = false;
    ResetCause cached_cause = ResetCause::UNKNOWN;
    bool       cause_cached = false;
};

inline Mgm240WatchdogState& mgm240WatchdogState() {
    static Mgm240WatchdogState s{};  // okay static in header — single-TU `.impl.hpp`
    return s;
}

inline ResetCause translateMgm240RstCause(fl::u32 cause) {
    // EFR32MG24 EMU_RSTCAUSE bits per reference manual.
    if (cause & EMU_RSTCAUSE_WDOG0)  return ResetCause::WATCHDOG;
    if (cause & EMU_RSTCAUSE_LOCKUP) return ResetCause::LOCKUP;
    if (cause & EMU_RSTCAUSE_SYSREQ) return ResetCause::SOFTWARE;
    if (cause & EMU_RSTCAUSE_PIN)    return ResetCause::EXTERNAL_PIN;
    if (cause & EMU_RSTCAUSE_POR)    return ResetCause::POWER_ON;
    return ResetCause::UNKNOWN;
}

// Map ms timeout to nearest `WDOG_PeriodSel_TypeDef`. LFRCO ~32.768 kHz; the
// period field is power-of-two counts: 9 (256 cycles ≈ 8 ms) up through
// 17 (65536 cycles ≈ 2 s) and beyond. Pick smallest period >= requested.
inline WDOG_PeriodSel_TypeDef mgm240PickPeriod(fl::u32 ms) {
    if (ms <=    8) return wdogPeriod_9;
    if (ms <=   16) return wdogPeriod_16;
    if (ms <=   32) return wdogPeriod_32;
    if (ms <=   64) return wdogPeriod_64;
    if (ms <=  125) return wdogPeriod_128;
    if (ms <=  250) return wdogPeriod_256;
    if (ms <=  500) return wdogPeriod_512;
    if (ms <= 1000) return wdogPeriod_1k;
    if (ms <= 2000) return wdogPeriod_2k;
    if (ms <= 4000) return wdogPeriod_4k;
    if (ms <= 8000) return wdogPeriod_8k;
    if (ms <=16000) return wdogPeriod_16k;
    return wdogPeriod_32k;
}

} // namespace platforms

Watchdog& Watchdog::instance() FL_NOEXCEPT {
    static Watchdog sInstance;
    return sInstance;
}

void Watchdog::begin(fl::u32 timeout_ms) FL_NOEXCEPT {
    if (timeout_ms == 0) timeout_ms = 1000;
    if (timeout_ms > FL_WATCHDOG_MAX_TIMEOUT_MS) timeout_ms = FL_WATCHDOG_MAX_TIMEOUT_MS;
    WDOG_Init_TypeDef init = WDOG_INIT_DEFAULT;
    init.perSel = platforms::mgm240PickPeriod(timeout_ms);
    WDOGn_Init(DEFAULT_WDOG, &init);
    platforms::mgm240WatchdogState().armed = true;
}

void Watchdog::feed() FL_NOEXCEPT {
    WDOGn_Feed(DEFAULT_WDOG);
}

void Watchdog::disable() FL_NOEXCEPT {
    WDOGn_Enable(DEFAULT_WDOG, false);
    platforms::mgm240WatchdogState().armed = false;
}

ResetCause Watchdog::lastResetCause() const FL_NOEXCEPT {
    auto& s = platforms::mgm240WatchdogState();
    if (!s.cause_cached) {
        s.cached_cause = platforms::translateMgm240RstCause(EMU_ResetCauseGet());
        EMU_ResetCauseClear();
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
    return platforms::mgm240WatchdogState().persist[idx];
}
void Watchdog::persistWrite(fl::size idx, fl::u8 v) FL_NOEXCEPT {
    if (idx >= FL_WATCHDOG_PERSIST_BYTES) return;
    platforms::mgm240WatchdogState().persist[idx] = v;
}

fl::u16 Watchdog::consecutiveCrashCount() const FL_NOEXCEPT {
    (void)lastResetCause();
    return platforms::mgm240WatchdogState().crash_count;
}
void Watchdog::markCleanShutdown() FL_NOEXCEPT { platforms::mgm240WatchdogState().crash_count = 0; }
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
