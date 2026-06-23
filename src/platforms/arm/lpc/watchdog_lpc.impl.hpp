// IWYU pragma: private

/// @file platforms/arm/lpc/watchdog_lpc.impl.hpp
/// @brief LPC8xx watchdog implementation — WWDT (Windowed Watchdog Timer).
///
/// Covers LPC84x family (LPC845-BRK bring-up target). Uses the internal
/// Watchdog Oscillator (WDOSC) — nominally 500 kHz, divided by 4 to give a
/// 125 kHz WWDT tick, so the 24-bit TC register lets us program timeouts
/// from ~32 µs to ~134 s. The defaults in `begin()` clamp to a 1-second
/// nominal window which is well within range.
///
/// **WDOSC accuracy:** the watchdog oscillator on LPC8xx is uncalibrated
/// silicon and can drift ±40% across voltage/temperature corners (per
/// UM11029 §4.6). That is fine for "panic reset after N seconds of hang" —
/// the failure mode is "device reboots a bit early or late" — but is not
/// a precision timer. Anyone needing accurate scheduling should NOT use
/// this as a wakeup source.
///
/// **Lock semantics:** the WWDT can be locked via MOD.LOCK (bit 5). Once
/// locked, MOD.WDEN cannot be cleared and TC cannot be changed until the
/// next reset. We deliberately do NOT set LOCK, leaving Phase-3-style
/// safety hardening up to the application.
///
/// **Persist storage** is in-RAM only — the LPC845 has no battery-backed
/// retention RAM. Crash counters survive a WWDT reset (RAM contents are
/// preserved across a chip reset on LPC84x) but NOT a power cycle.

#include "fl/wdt/watchdog.h"
#include "fl/stl/int.h"
#include "platforms/arm/lpc/is_lpc.h"

#define FL_WATCHDOG_HAS_HARDWARE
#define FL_WATCHDOG_PERSIST_BYTES 8
// 24-bit TC at the divided WDOSC rate. Even at the slowest realistic
// WDOSC corner (~280 kHz / 4 = 70 kHz tick) the TC ceiling is ~239 s;
// at the nominal 125 kHz tick it is ~134 s. Cap at 60 s so we stay
// comfortably inside the worst-case silicon corner.
#define FL_WATCHDOG_MAX_TIMEOUT_MS 60000u

namespace fl {
namespace platforms {

// ---------------------------------------------------------------------------
// LPC8xx register access. We deliberately do NOT depend on the LPC8xx
// Arduino core's headers for these — the variant header (LPC845.h) only
// exports the IRQ vector list, and pulling a CMSIS device header would tie
// this file to a specific SDK version. Inline volatile-pointer access keeps
// the dependency surface to "the chip is what UM11029 says it is".
//
// LPC84x register map (from NXP UM11029 §3, §4, §14):
//
//   WWDT base                       0x40000000
//     +0x000  TC      timeout count (24-bit)
//     +0x004  MOD     mode (WDEN=bit0, WDRESET=bit1, WDTOF=bit2, WDINT=bit3,
//                          WDPROTECT=bit4, LOCK=bit5)
//     +0x008  FEED    write 0xAA then 0x55 (no interrupt between)
//     +0x00C  TV      current count value (read-only)
//     +0x014  WARNINT warning interrupt threshold
//     +0x018  WINDOW  window value
//
//   SYSCON base                     0x40048000
//     +0x024  WDTOSCCTRL  WDOSC freq+divisor
//     +0x080  SYSAHBCLKCTRL0  bit 17 = WWDT clock enable
//     +0x0F0  SYSRSTSTAT  reset cause (write-1-to-clear)
//                          bit 0 POR, bit 1 EXTRST, bit 2 WDT, bit 3 BOD,
//                          bit 4 SYSRST
//     +0x238  PDRUNCFG    bit 6 = WDTOSC powerdown (0 = powered)
// ---------------------------------------------------------------------------

namespace lpc_wwdt_regs {
    constexpr fl::u32 kWwdtBase    = 0x40000000u;
    constexpr fl::u32 kSysconBase  = 0x40048000u;

    inline volatile fl::u32& reg(fl::u32 base, fl::u32 offset) {
        return *reinterpret_cast<volatile fl::u32*>(base + offset);  // ok reinterpret cast — MMIO register addressing
    }

    inline volatile fl::u32& TC()         { return reg(kWwdtBase,   0x000); }
    inline volatile fl::u32& MOD()        { return reg(kWwdtBase,   0x004); }
    inline volatile fl::u32& FEED()       { return reg(kWwdtBase,   0x008); }
    inline volatile fl::u32& WDTOSCCTRL() { return reg(kSysconBase, 0x024); }
    inline volatile fl::u32& SYSAHBCLK0() { return reg(kSysconBase, 0x080); }
    inline volatile fl::u32& SYSRSTSTAT() { return reg(kSysconBase, 0x0F0); }
    inline volatile fl::u32& PDRUNCFG()   { return reg(kSysconBase, 0x238); }

    constexpr fl::u32 kModWden     = (1u << 0);
    constexpr fl::u32 kModWdreset  = (1u << 1);
    constexpr fl::u32 kModWdtof    = (1u << 2);
    constexpr fl::u32 kModProtect  = (1u << 4);

    constexpr fl::u32 kSysAhbWwdt  = (1u << 17);
    constexpr fl::u32 kPdrunWdtosc = (1u << 6);

    constexpr fl::u32 kRstPor      = (1u << 0);
    constexpr fl::u32 kRstExt      = (1u << 1);
    constexpr fl::u32 kRstWdt      = (1u << 2);
    constexpr fl::u32 kRstBod      = (1u << 3);
    constexpr fl::u32 kRstSys      = (1u << 4);

    // WDOSC nominal tick after the WWDT-internal /4 prescaler. Per
    // UM11029 §4.6.7 Table 51, FREQSEL=0x2 selects a nominal 1.05 MHz
    // WDOSC source; with DIVSEL=0x00 the WDOSC output is 1.05/2 = 525 kHz
    // and the WWDT-internal /4 brings the tick to ~131 kHz. We program
    // kTickHz at 125 kHz (4-5% conservative) so TC values overshoot
    // slightly rather than undershoot, biasing the watchdog toward
    // "fires a little late" rather than "fires while legit code runs."
    // Either direction is within the ±40% silicon corner anyway.
    constexpr fl::u32 kTickHz      = 125000u;  // ~125 kHz after /4
    constexpr fl::u32 kWdtoscCtrl  = (0x2u << 5) | 0x00u;  // FREQSEL=2, DIVSEL=0
} // namespace lpc_wwdt_regs

struct LpcWatchdogState {
    fl::u8     persist[FL_WATCHDOG_PERSIST_BYTES] = {0};
    fl::u16    crash_count = 0;
    bool       armed = false;
    ResetCause cached_cause = ResetCause::UNKNOWN;
    bool       cause_cached = false;
};

inline LpcWatchdogState& lpcWatchdogState() {
    static LpcWatchdogState s{};  // okay static in header — single-TU `.impl.hpp`
    return s;
}

inline void lpcWwdtFeed() {
    // FEED sequence must be 0xAA then 0x55 with no other WWDT register write
    // (and ideally no interrupt) in between. The LPC845 hardware tolerates
    // interrupts mid-sequence as long as nothing else touches WWDT, but
    // belt-and-suspenders: disable IRQs.
    fl::u32 primask;
    __asm volatile("MRS %0, PRIMASK\n\tCPSID i" : "=r"(primask) :: "memory");
    lpc_wwdt_regs::FEED() = 0xAAu;
    lpc_wwdt_regs::FEED() = 0x55u;
    __asm volatile("MSR PRIMASK, %0" :: "r"(primask) : "memory");
}

} // namespace platforms

Watchdog& Watchdog::instance() FL_NOEXCEPT {
    static Watchdog sInstance;
    return sInstance;
}

void Watchdog::begin(fl::u32 timeout_ms) FL_NOEXCEPT {
    using namespace platforms::lpc_wwdt_regs;
    if (timeout_ms == 0) timeout_ms = 1000;
    if (timeout_ms > FL_WATCHDOG_MAX_TIMEOUT_MS) timeout_ms = FL_WATCHDOG_MAX_TIMEOUT_MS;

    // Power up the watchdog oscillator and turn on its AHB clock. The bit
    // for the WDOSC powerdown is *cleared* to enable (PDRUNCFG is an
    // active-low powerdown register).
    PDRUNCFG()   &= ~kPdrunWdtosc;
    SYSAHBCLK0() |=  kSysAhbWwdt;

    // Set WDOSC frequency band. Per UM11029 §4.6 the WDOSC frequency is
    // somewhere between FREQSEL*DIVSEL nominal and ±40% corner — we want a
    // round number so the math here matches WWDT_TC units.
    WDTOSCCTRL() = kWdtoscCtrl;

    // TC is in WWDT ticks; we are at ~125 kHz after the internal /4. Round
    // up so we never timeout EARLY (timeout_ms is the maximum allowed
    // silence between feeds, undershooting it would crash live code).
    fl::u32 ticks = (timeout_ms * kTickHz + 999u) / 1000u;
    if (ticks < 0x100u)      ticks = 0x100u;       // hardware minimum
    if (ticks > 0x00FFFFFFu) ticks = 0x00FFFFFFu;  // 24-bit ceiling
    TC() = ticks;

    // MOD: WDEN (enable) + WDRESET (reset on timeout, NOT interrupt). We
    // intentionally do NOT set LOCK — once locked WDEN cannot be cleared,
    // which makes step-debugging painful. The user can lock it from their
    // sketch if they want production hardening.
    MOD() = kModWden | kModWdreset;

    // Feed once to commit the new TC value and start counting.
    platforms::lpcWwdtFeed();

    platforms::lpcWatchdogState().armed = true;
}

void Watchdog::feed() FL_NOEXCEPT {
    if (platforms::lpcWatchdogState().armed) {
        platforms::lpcWwdtFeed();
    }
}

void Watchdog::disable() FL_NOEXCEPT {
    // WWDT cannot be disabled once WDEN is set without resetting the chip,
    // unless LOCK is also clear (it is — see begin()). The "soft disable"
    // semantics our caller expects are "stop feeding" — we still need the
    // hardware enabled to detect a real hang, so just mark armed=false and
    // let the existing watchdog reset the chip if the caller stops feeding.
    // This matches the STM32 IWDG behavior (also un-stoppable).
    platforms::lpcWatchdogState().armed = false;
}

ResetCause Watchdog::lastResetCause() const FL_NOEXCEPT {
    using namespace platforms::lpc_wwdt_regs;
    auto& s = platforms::lpcWatchdogState();
    if (!s.cause_cached) {
        const fl::u32 stat = SYSRSTSTAT();

        // Priority: WDT > BOD > SYSRESETREQ > external pin > POR. (Same
        // "specific first" priority STM32 uses.)
        ResetCause cause = ResetCause::UNKNOWN;
        if      (stat & kRstWdt) cause = ResetCause::WATCHDOG;
        else if (stat & kRstBod) cause = ResetCause::BROWNOUT;
        else if (stat & kRstSys) cause = ResetCause::SOFTWARE;
        else if (stat & kRstExt) cause = ResetCause::EXTERNAL_PIN;
        else if (stat & kRstPor) cause = ResetCause::POWER_ON;

        // SYSRSTSTAT bits are sticky and write-1-to-clear. Clear them so
        // the next boot only sees its own cause.
        SYSRSTSTAT() = stat;

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
    return platforms::lpcWatchdogState().persist[idx];
}
void Watchdog::persistWrite(fl::size idx, fl::u8 v) FL_NOEXCEPT {
    if (idx >= FL_WATCHDOG_PERSIST_BYTES) return;
    platforms::lpcWatchdogState().persist[idx] = v;
}

fl::u16 Watchdog::consecutiveCrashCount() const FL_NOEXCEPT {
    (void)lastResetCause();
    return platforms::lpcWatchdogState().crash_count;
}
void Watchdog::markCleanShutdown() FL_NOEXCEPT { platforms::lpcWatchdogState().crash_count = 0; }
bool Watchdog::isInSafeMode() const FL_NOEXCEPT { return consecutiveCrashCount() >= mSafeModeThreshold; }
fl::u16 Watchdog::safeModeThreshold() const FL_NOEXCEPT { return mSafeModeThreshold; }
void    Watchdog::setSafeModeThreshold(fl::u16 t) FL_NOEXCEPT { mSafeModeThreshold = t; }

FL_NORETURN void Watchdog::reboot() FL_NOEXCEPT {
    // Cortex-M0+ AIRCR.SYSRESETREQ — same write CMSIS NVIC_SystemReset()
    // uses, expanded inline so we don't need to pull in CMSIS headers.
    volatile fl::u32* aircr = reinterpret_cast<volatile fl::u32*>(0xE000ED0Cu);  // ok reinterpret cast — Cortex-M AIRCR MMIO
    *aircr = (0x05FAu << 16) | (1u << 2);
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
