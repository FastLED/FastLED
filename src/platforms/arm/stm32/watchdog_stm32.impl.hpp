// IWYU pragma: private

/// @file platforms/arm/stm32/watchdog_stm32.impl.hpp
/// @brief STM32 watchdog implementation — IWDG (Independent Watchdog).
///
/// Uses the LSI-clocked IWDG (~32 kHz, ±50%). Range ~125 µs to 32 s.
/// Cannot be stopped once started. Reset cause from `RCC->CSR` (F1/F4/L4/
/// G0/G4) or `RCC->RSR` (H7).
///
/// **STM32duino dependency:** Uses HAL `IWDG_HandleTypeDef` directly via
/// the LL/HAL headers exposed through `stm32_def.h` in the Arduino_Core_STM32
/// installation. The mbed-OS variants (Giga R1, Portenta) have a different
/// watchdog wrapper that this impl does NOT cover — falls back to noop on
/// those builds.
///
/// Persist storage is in-RAM only; Phase 3 follow-up can move it to RTC
/// backup registers (`RTC->BKPxR`) for VBAT-backed power-cycle survival.

#include "fl/wdt/watchdog.h"
#include "platforms/arm/stm32/is_stm32.h"

// IWYU pragma: begin_keep
#include <IWatchdog.h>   // ok include — STM32duino bundled watchdog wrapper
// IWYU pragma: end_keep

#define FL_WATCHDOG_HAS_HARDWARE
#define FL_WATCHDOG_PERSIST_BYTES 8
#define FL_WATCHDOG_MAX_TIMEOUT_MS 32000u

namespace fl {
namespace platforms {

struct Stm32WatchdogState {
    fl::u8     persist[FL_WATCHDOG_PERSIST_BYTES] = {0};
    fl::u16    crash_count = 0;
    bool       armed = false;
    ResetCause cached_cause = ResetCause::UNKNOWN;
    bool       cause_cached = false;
};

inline Stm32WatchdogState& stm32WatchdogState() {
    static Stm32WatchdogState s{};  // okay static in header — single-TU `.impl.hpp`
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
    // STM32duino IWatchdog takes microseconds.
    IWatchdog.begin(static_cast<fl::u32>(timeout_ms) * 1000u);
    platforms::stm32WatchdogState().armed = true;
}

void Watchdog::feed() FL_NOEXCEPT {
    IWatchdog.reload();
}

void Watchdog::disable() FL_NOEXCEPT {
    // IWDG cannot be stopped once started. No-op.
}

ResetCause Watchdog::lastResetCause() const FL_NOEXCEPT {
    auto& s = platforms::stm32WatchdogState();
    if (!s.cause_cached) {
        // Read the RCC reset-cause register. STM32 H7 uses `RCC->RSR`,
        // every other family in scope (F1/F4/F7/L4/G0/G4) uses `RCC->CSR`.
        // The flag-bit layout is consistent across the families we cover.
        #if defined(FL_IS_STM32_H7)
            const fl::u32 csr = RCC->RSR;
        #else
            const fl::u32 csr = RCC->CSR;
        #endif

        // Bit positions per STM32 reference manuals. "Specific first"
        // priority: WDT before brown-out, brown-out before generic POR.
        ResetCause cause = ResetCause::UNKNOWN;
        #ifdef RCC_CSR_IWDGRSTF
            if      (csr & RCC_CSR_IWDGRSTF) cause = ResetCause::WATCHDOG;
        #endif
        #ifdef RCC_CSR_WWDGRSTF
            else if (csr & RCC_CSR_WWDGRSTF) cause = ResetCause::WATCHDOG;
        #endif
        #ifdef RCC_CSR_SFTRSTF
            else if (csr & RCC_CSR_SFTRSTF)  cause = ResetCause::SOFTWARE;
        #endif
        #ifdef RCC_CSR_BORRSTF
            else if (csr & RCC_CSR_BORRSTF)  cause = ResetCause::BROWNOUT;
        #endif
        #ifdef RCC_CSR_PINRSTF
            else if (csr & RCC_CSR_PINRSTF)  cause = ResetCause::EXTERNAL_PIN;
        #endif
        #ifdef RCC_CSR_LPWRRSTF
            else if (csr & RCC_CSR_LPWRRSTF) cause = ResetCause::UNKNOWN;
        #endif
        #ifdef RCC_CSR_PORRSTF
            else if (csr & RCC_CSR_PORRSTF)  cause = ResetCause::POWER_ON;
        #endif

        // STM32 reset flags are sticky — clear them so the NEXT boot only
        // sees its own cause. RMVF (Remove flags) bit is in CSR/RSR; the
        // exact name varies slightly across families.
        #if defined(RCC_CSR_RMVF)
            RCC->CSR |= RCC_CSR_RMVF;
        #elif defined(RCC_RSR_RMVF)
            RCC->RSR |= RCC_RSR_RMVF;
        #endif

        s.cached_cause = cause;
        s.cause_cached = true;
        if (cause == ResetCause::WATCHDOG && s.crash_count < 0xFFFF) {
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
    return platforms::stm32WatchdogState().persist[idx];
}
void Watchdog::persistWrite(fl::size idx, fl::u8 v) FL_NOEXCEPT {
    if (idx >= FL_WATCHDOG_PERSIST_BYTES) return;
    platforms::stm32WatchdogState().persist[idx] = v;
}

fl::u16 Watchdog::consecutiveCrashCount() const FL_NOEXCEPT {
    (void)lastResetCause();
    return platforms::stm32WatchdogState().crash_count;
}
void Watchdog::markCleanShutdown() FL_NOEXCEPT { platforms::stm32WatchdogState().crash_count = 0; }
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
