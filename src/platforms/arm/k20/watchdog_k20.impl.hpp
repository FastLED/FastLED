// IWYU pragma: private

/// @file platforms/arm/k20/watchdog_k20.impl.hpp
/// @brief Teensy 3.x (K20/K66/KL26) watchdog implementation.
///
/// Direct `WDOG` register access via the Kinetis SDK. **Configuration is
/// one-shot** within ~256 bus cycles of the unlock sequence; conventionally
/// done from `startup_early_hook()` before `main()`. Since we cannot reliably
/// intercept that hook without conflicting with sketches that override it,
/// `begin()` only refreshes the prescaler/timeout values via the unlock
/// window — if Teensyduino's default `startup_early_hook()` already wrote
/// `ALLOWUPDATE`, this works. Otherwise `begin()` is a no-op.
///
/// Reset cause from `RCM_SRS0`/`RCM_SRS1`. Persist storage is in-RAM only;
/// Phase 3 follow-up can move it to a custom `.noinit` SRAM section.

#include "fl/wdt/watchdog.h"

// IWYU pragma: begin_keep
#include <kinetis.h>   // ok include — WDOG / RCM register macros
// IWYU pragma: end_keep

#define FL_WATCHDOG_HAS_HARDWARE
#define FL_WATCHDOG_PERSIST_BYTES 8
#define FL_WATCHDOG_MAX_TIMEOUT_MS 60000u

namespace fl {
namespace platforms {

struct K20WatchdogState {
    fl::u8     persist[FL_WATCHDOG_PERSIST_BYTES] = {0};
    fl::u16    crash_count = 0;
    bool       armed = false;
    ResetCause cached_cause = ResetCause::UNKNOWN;
    bool       cause_cached = false;
};

inline K20WatchdogState& k20WatchdogState() {
    static K20WatchdogState s{};  // okay static in header — single-TU `.impl.hpp`
    return s;
}

// Translate RCM_SRS0/SRS1 to fl::ResetCause. K20/K66 priority: most-specific first.
inline ResetCause translateRcmSrs(fl::u8 srs0, fl::u8 srs1) {
    (void)srs1;  // SRS1 bits (LOCKUP/SW/MDM_AP/SACKERR) handled below
    if (srs0 & 0x20) return ResetCause::WATCHDOG;       // RCM_SRS0_WDOG
    if (srs0 & 0x02) return ResetCause::BROWNOUT;       // RCM_SRS0_LVD
    if (srs0 & 0x40) return ResetCause::EXTERNAL_PIN;   // RCM_SRS0_PIN
    if (srs0 & 0x80) return ResetCause::POWER_ON;       // RCM_SRS0_POR
    if (srs1 & 0x02) return ResetCause::LOCKUP;         // RCM_SRS1_LOCKUP
    if (srs1 & 0x04) return ResetCause::SOFTWARE;       // RCM_SRS1_SW
    if (srs1 & 0x08) return ResetCause::DEBUGGER;       // RCM_SRS1_MDM_AP
    return ResetCause::UNKNOWN;
}

inline void k20UnlockAndConfigureWdog(fl::u32 timeout_ms) {
    // Standard Kinetis WDOG unlock sequence: write 0xC520 then 0xD928 to
    // WDOG_UNLOCK within bus-clock window. Then exactly ONE write to
    // WDOG_STCTRLH is permitted before lockout. We program LPO-clocked
    // (~1 kHz) so timeout_ms ≈ TOVAL count.
    //
    // PRIMASK save/restore so a caller that already had IRQs disabled
    // doesn't get them surprise-re-enabled. Naked __disable_irq() pair
    // would lose the prior PRIMASK state.
    fl::u32 primask;
    __asm__ volatile ("mrs %0, primask" : "=r" (primask));
    __asm__ volatile ("cpsid i" ::: "memory");
    WDOG_UNLOCK = 0xC520;
    WDOG_UNLOCK = 0xD928;
    // Two NOPs per the reference manual to satisfy the timing requirement.
    __asm__ volatile ("nop");
    __asm__ volatile ("nop");
    fl::u32 toval = timeout_ms;
    if (toval == 0) toval = 1;
    WDOG_TOVALH = static_cast<fl::u16>((toval >> 16) & 0xFFFF);
    WDOG_TOVALL = static_cast<fl::u16>(toval & 0xFFFF);
    // STCTRLH: ALLOWUPDATE | WDOGEN | STOPEN | WAITEN | LPO clock = 0x01D1.
    // The STOPEN + WAITEN bits keep the WDT counting when the CPU enters
    // STOP / WAIT modes — without them the WDT would pause and the
    // caller's feed() cadence would be unsafe across low-power transitions.
    WDOG_STCTRLH = 0x01D1;
    __asm__ volatile ("msr primask, %0" :: "r" (primask) : "memory");
}

} // namespace platforms

Watchdog& Watchdog::instance() FL_NOEXCEPT {
    static Watchdog sInstance;
    return sInstance;
}

void Watchdog::begin(fl::u32 timeout_ms) FL_NOEXCEPT {
    if (timeout_ms == 0) timeout_ms = 1000;
    if (timeout_ms > FL_WATCHDOG_MAX_TIMEOUT_MS) timeout_ms = FL_WATCHDOG_MAX_TIMEOUT_MS;
    platforms::k20UnlockAndConfigureWdog(timeout_ms);
    platforms::k20WatchdogState().armed = true;
}

void Watchdog::feed() FL_NOEXCEPT {
    // Refresh sequence: 0xA602 then 0xB480 with interrupts disabled.
    // PRIMASK save/restore so a caller that already had IRQs disabled
    // doesn't get them surprise-re-enabled (e.g. feed() from inside an ISR).
    fl::u32 primask;
    __asm__ volatile ("mrs %0, primask" : "=r" (primask));
    __asm__ volatile ("cpsid i" ::: "memory");
    WDOG_REFRESH = 0xA602;
    WDOG_REFRESH = 0xB480;
    __asm__ volatile ("msr primask, %0" :: "r" (primask) : "memory");
}

void Watchdog::disable() FL_NOEXCEPT {
    // K20 WDOG is one-shot configurable. We cannot reliably disable mid-run.
    // Document the limitation: post-disable() the watchdog still ticks. The
    // caller must continue feeding it or accept a reset.
}

ResetCause Watchdog::lastResetCause() const FL_NOEXCEPT {
    auto& s = platforms::k20WatchdogState();
    if (!s.cause_cached) {
        s.cached_cause = platforms::translateRcmSrs(RCM_SRS0, RCM_SRS1);
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
    return platforms::k20WatchdogState().persist[idx];
}
void Watchdog::persistWrite(fl::size idx, fl::u8 v) FL_NOEXCEPT {
    if (idx >= FL_WATCHDOG_PERSIST_BYTES) return;
    platforms::k20WatchdogState().persist[idx] = v;
}

fl::u16 Watchdog::consecutiveCrashCount() const FL_NOEXCEPT {
    (void)lastResetCause();
    return platforms::k20WatchdogState().crash_count;
}
void Watchdog::markCleanShutdown() FL_NOEXCEPT { platforms::k20WatchdogState().crash_count = 0; }
bool Watchdog::isInSafeMode() const FL_NOEXCEPT { return consecutiveCrashCount() >= mSafeModeThreshold; }
fl::u16 Watchdog::safeModeThreshold() const FL_NOEXCEPT { return mSafeModeThreshold; }
void    Watchdog::setSafeModeThreshold(fl::u16 t) FL_NOEXCEPT { mSafeModeThreshold = t; }

FL_NORETURN void Watchdog::reboot() FL_NOEXCEPT {
    SCB_AIRCR = 0x05FA0004;
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
