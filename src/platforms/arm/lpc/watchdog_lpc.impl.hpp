// IWYU pragma: private

/// @file platforms/arm/lpc/watchdog_lpc.impl.hpp
/// @brief LPC8xx watchdog implementation — WWDT (Windowed Watchdog Timer).
///
/// Covers LPC845 and LPC804 (the two chips whose vendor CMSIS PAL ships in
/// the ArduinoCore-LPC8xx variant tree — `<LPC845.h>` / `<LPC804.h>`). All
/// register access goes through the vendor `WWDT_Type` / `SYSCON_Type`
/// structs per the #3437 "no hand-rolled register maps" policy. The previous
/// revision of this file hand-rolled the WWDT offsets and had MOD and TC
/// swapped (TC written to offset 0x0 which is MOD, MOD written to 0x4 which
/// is TC) — the watchdog silently never armed. Silicon-diagnosed on the
/// LPC845-BRK 2026-07-02: a deliberately wedged RPC handler ran forever
/// with a 3 s watchdog "armed". Vendor-struct access makes that class of
/// bug unrepresentable. LPC11xx / LPC15xx have no vendor PAL in-tree and
/// different SYSCON layouts; they fall through to the no-op backend (see
/// `platforms/watchdog.impl.cpp.hpp`).
///
/// Uses the internal Watchdog Oscillator (WDOSC) — nominally 525 kHz after
/// DIVSEL, divided by 4 inside the WWDT to give a ~131 kHz tick (we budget
/// 125 kHz), so the 24-bit TC register lets us program timeouts from ~32 µs
/// to ~134 s. The defaults in `begin()` clamp to a 1-second nominal window.
///
/// **WDOSC accuracy:** the watchdog oscillator on LPC8xx is uncalibrated
/// silicon and can drift ±40% across voltage/temperature corners (per
/// UM11029 §4.6). That is fine for "panic reset after N seconds of hang" —
/// the failure mode is "device reboots a bit early or late" — but is not
/// a precision timer. Anyone needing accurate scheduling should NOT use
/// this as a wakeup source.
///
/// **Wedge backtrace (NMI warning hook):** `begin()` programs the WWDT
/// warning threshold (WARNINT, 10-bit → fires ~8 ms before the reset at
/// the 125 kHz tick) and routes the WDT interrupt line to the Cortex-M0+
/// NMI via `SYSCON->NMISRC`. The ACLPC startup file's `NMI_Handler` is a
/// STRONG alias of `HardFault_Handler` (not overridable from here — a
/// weak-alias upstreaming is possible later), and that shared handler
/// already prints the stacked exception frame and resets. So when
/// firmware wedges (stops feeding), the warning fires NMI and the core
/// emits `FAULT: HardFault PC=<addr> LR=<addr> xPSR=<addr>` over USART0.
/// All three values are the stacked exception frame of the WEDGED context
/// (ARMv6-M B1.5.4), so PC is the address the firmware was stuck at —
/// symbolicate it against `firmware.map` (the autoresearch build always
/// emits one). Silicon-validated 2026-07-02: a deliberately wedged RPC
/// handler produced `FAULT: HardFault PC=0x100018BC ...` at t=+3.0 s,
/// the PC symbolicated to the exact static object the driver had jumped
/// into, and the chip reset and answered RPC echo again immediately.
///
/// Two consequences of reusing the core handler: (1) the reboot happens
/// via SYSRESETREQ inside that handler, so `lastResetCause()` reports
/// SOFTWARE rather than WATCHDOG for a wedge caught by the warning hook
/// (a wedge that somehow skips the NMI still hard-resets ~8 ms later
/// with an authentic WDT cause); (2) `consecutiveCrashCount()` does not
/// increment for warning-hook resets. Opt out of the hook entirely with
/// `-DFL_LPC_WDT_NMI_BACKTRACE=0` (frees NMISRC for app use and restores
/// authentic WDT reset attribution).
///
/// **Lock semantics:** the WWDT can be locked via MOD.LOCK (bit 5). Once
/// locked, MOD.WDEN cannot be cleared and TC cannot be changed until the
/// next reset. We deliberately do NOT set LOCK, leaving Phase-3-style
/// safety hardening up to the application.
///
/// **Persist storage** is in-RAM `.bss` — zeroed by startup on every reset,
/// so it does NOT survive a WWDT reset (the ACLPC linker script has no
/// `.noinit` section to place it in). The wedge PC/LR are therefore
/// printed live at wedge time rather than recovered after reboot.

#include "fl/wdt/watchdog.h"
#include "fl/stl/int.h"
#include "platforms/arm/is_arm.h"

// IWYU pragma: begin_keep
#if defined(FL_IS_ARM_LPC_845)
#include <LPC845.h>
#elif defined(FL_IS_ARM_LPC_804)
#include <LPC804.h>
#else
#error "watchdog_lpc.impl.hpp requires the LPC845/LPC804 vendor CMSIS PAL; \
gate the include in platforms/watchdog.impl.cpp.hpp on FL_IS_ARM_LPC_845 || \
FL_IS_ARM_LPC_804 (LPC11xx/LPC15xx take the no-op backend)."
#endif
// IWYU pragma: end_keep

#ifndef FL_LPC_WDT_NMI_BACKTRACE
#define FL_LPC_WDT_NMI_BACKTRACE 1
#endif

#define FL_WATCHDOG_HAS_HARDWARE
#define FL_WATCHDOG_PERSIST_BYTES 8
// 24-bit TC at the divided WDOSC rate. Even at the slowest realistic
// WDOSC corner (~280 kHz / 4 = 70 kHz tick) the TC ceiling is ~239 s;
// at the nominal 125 kHz tick it is ~134 s. Cap at 60 s so we stay
// comfortably inside the worst-case silicon corner.
#define FL_WATCHDOG_MAX_TIMEOUT_MS 60000u

namespace fl {
namespace platforms {

namespace lpc_wwdt {
    // WDOSC nominal tick after the WWDT-internal /4 prescaler. Per
    // UM11029 §4.6.7 Table 51, FREQSEL=0x2 selects a nominal 1.05 MHz
    // WDOSC source; with DIVSEL=0x00 the WDOSC output is 1.05/2 = 525 kHz
    // and the WWDT-internal /4 brings the tick to ~131 kHz. We budget
    // kTickHz at 125 kHz (4-5% conservative) so TC values overshoot
    // slightly rather than undershoot, biasing the watchdog toward
    // "fires a little late" rather than "fires while legit code runs."
    // Either direction is within the ±40% silicon corner anyway.
    constexpr fl::u32 kTickHz = 125000u;
    constexpr fl::u32 kWdtoscCtrl =
        SYSCON_WDTOSCCTRL_FREQSEL(0x2u) | SYSCON_WDTOSCCTRL_DIVSEL(0x00u);

    constexpr fl::u32 kModWden    = WWDT_MOD_WDEN_MASK;
    constexpr fl::u32 kModWdreset = WWDT_MOD_WDRESET_MASK;
    constexpr fl::u32 kModWdint   = WWDT_MOD_WDINT_MASK;

    // SYSRSTSTAT cause bits (UM11029 §6.6.15).
    constexpr fl::u32 kRstPor = (1u << 0);
    constexpr fl::u32 kRstExt = (1u << 1);
    constexpr fl::u32 kRstWdt = (1u << 2);
    constexpr fl::u32 kRstBod = (1u << 3);
    constexpr fl::u32 kRstSys = (1u << 4);
} // namespace lpc_wwdt

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
    // belt-and-suspenders: disable IRQs. (The NMI warning hook can still
    // preempt this — harmless, it only reads WWDT->MOD.)
    fl::u32 primask;
    __asm volatile("MRS %0, PRIMASK\n\tCPSID i" : "=r"(primask) :: "memory");
    WWDT->FEED = 0xAAu;
    WWDT->FEED = 0x55u;
    __asm volatile("MSR PRIMASK, %0" :: "r"(primask) : "memory");
}

} // namespace platforms

Watchdog& Watchdog::instance() FL_NO_EXCEPT {
    static Watchdog sInstance;
    return sInstance;
}

void Watchdog::begin(fl::u32 timeout_ms) FL_NO_EXCEPT {
    using namespace platforms::lpc_wwdt;
    if (timeout_ms == 0) timeout_ms = 1000;
    if (timeout_ms > FL_WATCHDOG_MAX_TIMEOUT_MS) timeout_ms = FL_WATCHDOG_MAX_TIMEOUT_MS;

    // Power up the watchdog oscillator and turn on the WWDT AHB clock. The
    // WDTOSC powerdown bit is *cleared* to enable (PDRUNCFG is an
    // active-low powerdown register).
    SYSCON->PDRUNCFG      &= ~SYSCON_PDRUNCFG_WDTOSC_PD_MASK;
    SYSCON->SYSAHBCLKCTRL0 |= SYSCON_SYSAHBCLKCTRL0_WWDT_MASK;

    // Set WDOSC frequency band. Per UM11029 §4.6 the WDOSC frequency is
    // somewhere between the FREQSEL/DIVSEL nominal and a ±40% corner — we
    // want a round number so the math here matches WWDT->TC units.
    SYSCON->WDTOSCCTRL = kWdtoscCtrl;

    // TC is in WWDT ticks; we are at ~125 kHz after the internal /4. Round
    // up so we never timeout EARLY (timeout_ms is the maximum allowed
    // silence between feeds, undershooting it would crash live code).
    fl::u32 ticks = (timeout_ms * kTickHz + 999u) / 1000u;
    if (ticks < 0x100u)      ticks = 0x100u;       // hardware minimum
    if (ticks > 0x00FFFFFFu) ticks = 0x00FFFFFFu;  // 24-bit ceiling
    WWDT->TC = ticks;

#if FL_LPC_WDT_NMI_BACKTRACE
    // Warning threshold: fire the WDT interrupt when the counter reaches
    // the 10-bit maximum (1023 ticks ≈ 8 ms at 125 kHz) so the NMI hook
    // below gets a window to print the wedged PC before the hard reset.
    WWDT->WARNINT = WWDT_WARNINT_WARNINT_MASK;
#endif

    // MOD: WDEN (enable) + WDRESET (reset on timeout). Writing bit 3
    // (WDINT) as 1 also W1C-clears any stale warning flag from a previous
    // session so routing the line to NMI below can't fire spuriously;
    // bit 2 (WDTOF) written 0 clears the stale timeout flag. We
    // intentionally do NOT set LOCK — once locked WDEN cannot be cleared,
    // which makes step-debugging painful. The user can lock it from their
    // sketch if they want production hardening.
    WWDT->MOD = kModWden | kModWdreset | kModWdint;

    // Feed once to commit the new TC value and start counting.
    platforms::lpcWwdtFeed();

#if FL_LPC_WDT_NMI_BACKTRACE
    // Route the WDT interrupt line to NMI. The chip-level IRQ slots in
    // the ACLPC vector table are all Default_Handler (no per-IRQ weak
    // aliases), but the core's NMI_Handler — a strong alias of its
    // HardFault_Handler — already prints the stacked PC/LR over USART0
    // and resets. That IS the wedge backtrace (see file header).
    SYSCON->NMISRC = SYSCON_NMISRC_NMIEN_MASK | SYSCON_NMISRC_IRQN(WDT_IRQn);
#endif

    platforms::lpcWatchdogState().armed = true;
}

void Watchdog::feed() FL_NO_EXCEPT {
    if (platforms::lpcWatchdogState().armed) {
        platforms::lpcWwdtFeed();
    }
}

void Watchdog::disable() FL_NO_EXCEPT {
    // WWDT cannot be disabled once WDEN is set without resetting the chip,
    // unless LOCK is also clear (it is — see begin()). The "soft disable"
    // semantics our caller expects are "stop feeding" — we still need the
    // hardware enabled to detect a real hang, so just mark armed=false and
    // let the existing watchdog reset the chip if the caller stops feeding.
    // This matches the STM32 IWDG behavior (also un-stoppable).
    platforms::lpcWatchdogState().armed = false;
}

ResetCause Watchdog::lastResetCause() const FL_NO_EXCEPT {
    using namespace platforms::lpc_wwdt;
    auto& s = platforms::lpcWatchdogState();
    if (!s.cause_cached) {
        const fl::u32 stat = SYSCON->SYSRSTSTAT;

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
        SYSCON->SYSRSTSTAT = stat;

        s.cached_cause = cause;
        s.cause_cached = true;
        if (cause == ResetCause::WATCHDOG && s.crash_count < 0xFFFF) {
            s.crash_count++;
        }
    }
    return s.cached_cause;
}

bool Watchdog::lastResetWasWatchdog() const FL_NO_EXCEPT {
    return lastResetCause() == ResetCause::WATCHDOG;
}

fl::u8 Watchdog::persistRead(fl::size idx) const FL_NO_EXCEPT {
    if (idx >= FL_WATCHDOG_PERSIST_BYTES) return 0;
    return platforms::lpcWatchdogState().persist[idx];
}
void Watchdog::persistWrite(fl::size idx, fl::u8 v) FL_NO_EXCEPT {
    if (idx >= FL_WATCHDOG_PERSIST_BYTES) return;
    platforms::lpcWatchdogState().persist[idx] = v;
}

fl::u16 Watchdog::consecutiveCrashCount() const FL_NO_EXCEPT {
    (void)lastResetCause();
    return platforms::lpcWatchdogState().crash_count;
}
void Watchdog::markCleanShutdown() FL_NO_EXCEPT { platforms::lpcWatchdogState().crash_count = 0; }
bool Watchdog::isInSafeMode() const FL_NO_EXCEPT { return consecutiveCrashCount() >= mSafeModeThreshold; }
fl::u16 Watchdog::safeModeThreshold() const FL_NO_EXCEPT { return mSafeModeThreshold; }
void    Watchdog::setSafeModeThreshold(fl::u16 t) FL_NO_EXCEPT { mSafeModeThreshold = t; }

FL_NO_RETURN void Watchdog::reboot() FL_NO_EXCEPT {
    // Cortex-M0+ AIRCR.SYSRESETREQ — same write CMSIS NVIC_SystemReset()
    // uses, expanded inline so we don't depend on core_cm0plus helpers.
    volatile fl::u32* aircr = reinterpret_cast<volatile fl::u32*>(0xE000ED0Cu);  // ok reinterpret cast — Cortex-M AIRCR MMIO
    *aircr = (0x05FAu << 16) | (1u << 2);
    while (true) {}
}

bool Watchdog::onTimeout(WatchdogTimeoutCallback, void*) FL_NO_EXCEPT { return false; }
bool Watchdog::onTimeout(fl::function<void()>) FL_NO_EXCEPT { return false; }
bool Watchdog::setPauseOnDebug(bool) FL_NO_EXCEPT { return false; }
bool Watchdog::writeCrashLog(fl::span<const fl::u8>) FL_NO_EXCEPT { return false; }
fl::size Watchdog::readCrashLog(fl::span<fl::u8>) const FL_NO_EXCEPT { return 0; }
bool Watchdog::rebootIntoBootloader() FL_NO_EXCEPT { return false; }

bool Watchdog::setWindow(fl::u32, fl::u32) FL_NO_EXCEPT { return false; }
bool Watchdog::hasCrashReport() const FL_NO_EXCEPT { return false; }
WatchdogCrashReport Watchdog::readCrashReport() const FL_NO_EXCEPT {
    WatchdogCrashReport r{}; r.valid = false; r.fault_type = ""; return r;
}
void Watchdog::clearCrashReport() FL_NO_EXCEPT {}

} // namespace fl
