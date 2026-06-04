// IWYU pragma: private

/// @file platforms/arm/nrf52/watchdog_nrf52.impl.hpp
/// @brief nRF52 (Adafruit BSP) watchdog implementation.
///
/// `NRF_WDT` registers — 32 kHz LFCLK with 32-bit CRV; range ~1 ms to 36 h.
/// Cannot be stopped once started. Reset cause from `NRF_POWER->RESETREAS`.
///
/// **GPREGRET caution:** The Adafruit nRF52 bootloader uses specific sentinel
/// values in `NRF_POWER->GPREGRET` (`0x57`, `0xA8`, `0x4e`, `0x90`, `0xB0`)
/// for DFU mode signaling. We use the user `persist` API for general state
/// and intentionally do NOT touch GPREGRET here to avoid conflicts.

#include "fl/wdt/watchdog.h"

// IWYU pragma: begin_keep
#include <nrf.h>   // ok include — NRF_WDT, NRF_POWER
// IWYU pragma: end_keep

#define FL_WATCHDOG_HAS_HARDWARE
#define FL_WATCHDOG_PERSIST_BYTES 8
#define FL_WATCHDOG_MAX_TIMEOUT_MS 600000u   // 10 min — well under 36 h ceiling

namespace fl {
namespace platforms {

struct Nrf52WatchdogState {
    fl::u8     persist[FL_WATCHDOG_PERSIST_BYTES] = {0};
    fl::u16    crash_count = 0;
    bool       armed = false;
    ResetCause cached_cause = ResetCause::UNKNOWN;
    bool       cause_cached = false;
};

inline Nrf52WatchdogState& nrf52WatchdogState() {
    static Nrf52WatchdogState s{};  // okay static in header — single-TU `.impl.hpp`
    return s;
}

inline ResetCause translateNrf52ResetReas(fl::u32 r) {
    // POWER->RESETREAS bits; multiple may be set — most-specific wins.
    if (r & 0x00000002u) return ResetCause::WATCHDOG;       // DOG
    if (r & 0x00000004u) return ResetCause::SOFTWARE;       // SREQ
    if (r & 0x00000008u) return ResetCause::LOCKUP;         // LOCKUP
    if (r & 0x00000001u) return ResetCause::EXTERNAL_PIN;   // RESETPIN
    if (r == 0)          return ResetCause::POWER_ON;       // all zero = POR
    return ResetCause::UNKNOWN;
}

} // namespace platforms

Watchdog& Watchdog::instance() FL_NOEXCEPT {
    static Watchdog sInstance;
    return sInstance;
}

void Watchdog::begin(fl::u32 timeout_ms) FL_NOEXCEPT {
    if (timeout_ms == 0) timeout_ms = 1000;
    if (timeout_ms > FL_WATCHDOG_MAX_TIMEOUT_MS) timeout_ms = FL_WATCHDOG_MAX_TIMEOUT_MS;
    if (NRF_WDT->RUNSTATUS == 0) {
        // CRV = (timeout_sec * 32768) - 1, computed from ms with rounding.
        NRF_WDT->CRV = ((static_cast<fl::u64>(timeout_ms) * 32768u + 999u) / 1000u) - 1u;
        NRF_WDT->RREN = 1u;            // enable reload register 0
        // CONFIG: bit 0 SLEEP (1 = run during CPU sleep), bit 3 HALT
        // (1 = pause when debugger has halted the CPU, developer-friendly).
        // Previously we wrote 1u which left HALT=0 → watchdog kept counting
        // through a debugger break and reset the chip mid-debug. Same class
        // of bit-semantic inversion that bit us on Teensy 4's WDOG_CS_DBG.
        NRF_WDT->CONFIG = (1u << 0) | (1u << 3);
        NRF_WDT->TASKS_START = 1u;
    }
    platforms::nrf52WatchdogState().armed = true;
}

void Watchdog::feed() FL_NOEXCEPT {
    NRF_WDT->RR[0] = 0x6E524635u;  // WDT_RR_RR_Reload magic
}

void Watchdog::disable() FL_NOEXCEPT {
    // nRF52 WDT cannot be disabled once started. Document and no-op.
}

ResetCause Watchdog::lastResetCause() const FL_NOEXCEPT {
    auto& s = platforms::nrf52WatchdogState();
    if (!s.cause_cached) {
        s.cached_cause = platforms::translateNrf52ResetReas(NRF_POWER->RESETREAS);
        // RESETREAS is sticky; write-1-to-clear so the next boot sees a
        // clean value.
        NRF_POWER->RESETREAS = 0xFFFFFFFFu;
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
    return platforms::nrf52WatchdogState().persist[idx];
}
void Watchdog::persistWrite(fl::size idx, fl::u8 v) FL_NOEXCEPT {
    if (idx >= FL_WATCHDOG_PERSIST_BYTES) return;
    platforms::nrf52WatchdogState().persist[idx] = v;
}

fl::u16 Watchdog::consecutiveCrashCount() const FL_NOEXCEPT {
    (void)lastResetCause();
    return platforms::nrf52WatchdogState().crash_count;
}
void Watchdog::markCleanShutdown() FL_NOEXCEPT { platforms::nrf52WatchdogState().crash_count = 0; }
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
