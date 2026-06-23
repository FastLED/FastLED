// IWYU pragma: private

/// @file platforms/arm/mxrt1062/watchdog_mxrt1062.impl.hpp
/// @brief Teensy 4.x (iMXRT1062) watchdog implementation.
///
/// **Bare-metal WDOG3 (RTWDOG) register access — no third-party library.**
/// Uses the iMXRT1062 RTWDOG peripheral via the register defines in `<imxrt.h>`
/// from the Teensyduino core. WDOG1/WDOG2 are intentionally NOT used — they're
/// configured by the ROM/bootloader and resetting through them can interact
/// with the MKL02 bootloader chip in ways the previous attempt (see
/// `AutoResearch.ino:204-214`) discovered the hard way.
///
/// **Register sequence (per iMXRT1062 reference manual, RTWDOG chapter):**
///
/// Reconfigure: write the 32-bit unlock key `0xD928C520` to `WDOG3_CNT`
/// (this requires `CS.CMD32EN=1`, which is the reset default). Wait for
/// `CS.ULK` to become 1, then program `TOVAL`/`WIN`, then write a new `CS`
/// that includes `CS.UPDATE` (to allow future reconfigures) and `CS.EN`.
/// Finally wait for `CS.RCS` to confirm the reconfigure completed.
///
/// Refresh: write the 32-bit refresh key `0xB480A602` to `WDOG3_CNT`.
///
/// Both sequences run with interrupts disabled — the unlock-to-write window
/// must complete within 16 bus clocks on the M7. At 600 MHz that's ample
/// time for the few stores below, but an ISR firing mid-sequence would
/// blow the window and either no-op or hang.
///
/// **Timing math:** LPO clock is 32 kHz nominal. We enable the /256 prescaler
/// (`CS.PRES`) so each TOVAL count is 8 ms. Max TOVAL is 0xFFFF → 524 sec
/// ceiling. We clamp `FL_WATCHDOG_MAX_TIMEOUT_MS` to 120 s for sanity.
///
/// Reset cause from `SRC_SRSR`. Persist storage is in-RAM only; Phase 3 can
/// move it to the OCRAM2 retained region above `CrashReport` at `0x2027FF80`.

#include "fl/wdt/watchdog.h"

// IWYU pragma: begin_keep
#include <imxrt.h>   // ok include — RTWDOG / SRC register defines from Teensyduino core
// IWYU pragma: end_keep

#define FL_WATCHDOG_HAS_HARDWARE
#define FL_WATCHDOG_PERSIST_BYTES 16
#define FL_WATCHDOG_MAX_TIMEOUT_MS 120000u

namespace fl {
namespace platforms {

// RTWDOG (WDOG3) magic keys per iMXRT1062 reference manual.
//
// The two 16-bit halves of the 32-bit unlock key:
//   high: 0xD928
//   low:  0xC520
// When `CS.CMD32EN=1` (the reset default) the single 32-bit store below is
// atomic to the WDT and replaces the two-word sequence used by older Kinetis
// WDOGs.
static constexpr fl::u32 kRtwdogUnlockKey  = 0xD928C520u;
static constexpr fl::u32 kRtwdogRefreshKey = 0xB480A602u;

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

inline ResetCause translateSrcSrsr(fl::u32 srsr) {
    // Bit positions from imxrt.h (which matches the iMXRT1062 reference
    // manual and NXP MCUXpresso SDK SRC_SRSR_*_SHIFT). Earlier hand-coded
    // shifts were all wrong (off by 1-9 bits in different places) so
    // lastResetCause() reported UNKNOWN for nearly every real reset cause.
    // Priority: most-specific first.
    if (srsr & SRC_SRSR_WDOG3_RST_B)        return ResetCause::WATCHDOG;
    if (srsr & SRC_SRSR_WDOG_RST_B)         return ResetCause::WATCHDOG;
    if (srsr & SRC_SRSR_LOCKUP_SYSRESETREQ) return ResetCause::LOCKUP;
    if (srsr & SRC_SRSR_JTAG_SW_RST)        return ResetCause::DEBUGGER;
    if (srsr & SRC_SRSR_IPP_USER_RESET_B)   return ResetCause::EXTERNAL_PIN;
    if (srsr & SRC_SRSR_IPP_RESET_B)        return ResetCause::POWER_ON;
    return ResetCause::UNKNOWN;
}

} // namespace platforms

Watchdog& Watchdog::instance() FL_NOEXCEPT {
    static Watchdog sInstance;
    return sInstance;
}

namespace platforms {

// First-call boot diagnostic. Prints the SRC_SRSR raw value + decoded bit
// names and the bundled Teensy `CrashReport` (if present) directly to
// `Serial`. Runs at most once per process. Called from `Watchdog::begin()`
// before any WDT writes so the user sees the prior-boot cause before the
// new WDT timer arms. SRC_SRSR is write-1-to-clear so the next boot reads
// only its own cause. This used to live behind `#if defined(FL_IS_TEENSY_4X)`
// in `examples/AutoResearch/AutoResearch.ino`; moving it into the platform
// impl lets every Teensy 4 sketch get the diagnostic for free by using
// `FL_WATCHDOG_AUTO()` or `FastLED.watchdog().begin()`.
inline void mxrt1062PrintBootDiagnosticOnce() {
    static bool sPrinted = false;  // okay static in header — single-TU `.impl.hpp`
    if (sPrinted) return;
    sPrinted = true;

    const fl::u32 srsr = SRC_SRSR;
    Serial.print("[FastLED.watchdog] SRC_SRSR = 0x");  // ok serial - platform-specific boot diagnostic
    Serial.println(srsr, HEX);                          // ok serial - platform-specific boot diagnostic
    if (srsr & SRC_SRSR_WDOG3_RST_B)        Serial.println("[FastLED.watchdog]   WDOG3_RST_B (RTWDOG fired)");        // ok serial
    if (srsr & SRC_SRSR_WDOG_RST_B)         Serial.println("[FastLED.watchdog]   WDOG_RST_B (WDOG1/2 fired)");        // ok serial
    if (srsr & SRC_SRSR_IPP_RESET_B)        Serial.println("[FastLED.watchdog]   POR (power-on reset)");              // ok serial
    if (srsr & SRC_SRSR_LOCKUP_SYSRESETREQ) Serial.println("[FastLED.watchdog]   LOCKUP_SYSRESETREQ (HardFault)");    // ok serial
    if (srsr & SRC_SRSR_IPP_USER_RESET_B)   Serial.println("[FastLED.watchdog]   IPP_USER_RESET_B (external pin)");   // ok serial
    if (srsr & SRC_SRSR_JTAG_SW_RST)        Serial.println("[FastLED.watchdog]   JTAG/SW reset");                     // ok serial
    SRC_SRSR = srsr;  // W1C
    if (CrashReport) {
        Serial.println("[FastLED.watchdog] *** CrashReport from previous boot: ***");  // ok serial
        Serial.print(CrashReport);                                                      // ok serial - bundled Teensy CrashReport
        Serial.println("[FastLED.watchdog] *** end CrashReport ***");                  // ok serial
    } else {
        Serial.println("[FastLED.watchdog] no CrashReport from previous boot");        // ok serial
    }
    Serial.flush();  // ok serial - flush the boot diagnostic before WDT arms
}

} // namespace platforms

void Watchdog::begin(fl::u32 timeout_ms) FL_NOEXCEPT {
    if (timeout_ms == 0) timeout_ms = 1000;
    if (timeout_ms > FL_WATCHDOG_MAX_TIMEOUT_MS) timeout_ms = FL_WATCHDOG_MAX_TIMEOUT_MS;

    // Print the boot diagnostic exactly once per process — before any WDT
    // writes so the user sees prior-boot cause + CrashReport before the
    // new timer arms.
    platforms::mxrt1062PrintBootDiagnosticOnce();

    // *** Enable the WDOG3 CCM clock gate FIRST. ***
    //
    // imxrt.h calls this out explicitly: "WDOG3 requires CCM_CCGR5_WDOG3".
    // Writes to WDOG3_* registers when the clock gate is off either get
    // silently ignored or cause a bus fault → HardFault → instant reset.
    // The previous WDOG3 init attempt (AutoResearch.ino TODO at lines 204-214)
    // almost certainly missed this. CCGR=3 → clock always on.
    CCM_CCGR5 |= CCM_CCGR5_WDOG3(3);
    // Per iMXRT1062 RM, ensure the CCM clock-gate store is visible to the
    // peripheral bus before the first WDOG3 access. NXP MCUXpresso's CCM
    // driver inserts these barriers after every CCGR write.
    __asm__ volatile ("dsb" ::: "memory");
    __asm__ volatile ("isb" ::: "memory");

    // Convert ms to TOVAL counts. With LPO (~32 kHz) and /256 prescaler
    // (CS.PRES set) the tick rate is 125 Hz → 8 ms per count. Clamp to the
    // 16-bit register range.
    fl::u32 toval = timeout_ms / 8u;
    if (toval == 0)         toval = 1;
    if (toval > 0xFFFFu)    toval = 0xFFFFu;

    // PRIMASK save/restore — see feed() for rationale (works correctly when
    // called from a context that already had IRQs disabled).
    fl::u32 primask;
    __asm__ volatile ("mrs %0, primask" : "=r" (primask));
    __asm__ volatile ("cpsid i" ::: "memory");

    // 32-bit unlock key (atomic store; relies on CS.CMD32EN=1, which is the
    // RTWDOG reset default and which we restore in the CS write below).
    WDOG3_CNT = platforms::kRtwdogUnlockKey;
    // Wait for the unlock window to open. The RM allows up to ~32 LPO ticks;
    // at 32 kHz that's ~1 ms, far longer than the bus stores below take.
    // Bounded spin to avoid an infinite loop if the unlock never lands
    // (e.g. CCM clock gate didn't get enabled for some reason).
    {
        fl::u32 spin = 0;
        while (!(WDOG3_CS & WDOG_CS_ULK)) {
            if (++spin > 1000000u) {
                __asm__ volatile ("msr primask, %0" :: "r" (primask) : "memory");
                return;
            }
        }
    }

    WDOG3_TOVAL = toval;
    WDOG3_WIN   = 0;   // No window mode — feeds at any time are valid.

    // Configure CS. Per iMXRT1062 RM 65.6.1, the STOP/WAIT/DBG bits mean
    // "enabled in stop/wait/debug mode", i.e. setting them keeps the WDT
    // counting in those modes. We deliberately leave all three clear so a
    // debugger session pauses the watchdog (developer-friendly) and so any
    // CPU power-down also pauses it.
    //  - EN        = 1  → enable RTWDOG
    //  - CMD32EN   = 1  → keep 32-bit unlock/refresh keying (matches the
    //                     CNT writes we use)
    //  - UPDATE    = 1  → allow future reconfigures via the unlock sequence
    //  - PRES      = 1  → /256 prescaler → 8 ms per TOVAL count
    //  - CLK = 0b01     → LPO clock source (~32 kHz)
    //  - STOP/WAIT/DBG = 0 → pause in stop/wait/debug modes
    WDOG3_CS = WDOG_CS_EN
             | WDOG_CS_CMD32EN
             | WDOG_CS_UPDATE
             | WDOG_CS_PRES
             | WDOG_CS_CLK(1);

    // Wait for the reconfigure to take effect (also bounded).
    {
        fl::u32 spin = 0;
        while (!(WDOG3_CS & WDOG_CS_RCS)) {
            if (++spin > 1000000u) {
                __asm__ volatile ("msr primask, %0" :: "r" (primask) : "memory");
                return;
            }
        }
    }

    // Restore the caller's prior PRIMASK state (NOT __enable_irq() — that
    // would unconditionally re-enable interrupts even if the caller had
    // them disabled before invoking begin()).
    __asm__ volatile ("msr primask, %0" :: "r" (primask) : "memory");

    auto& s = platforms::mxrt1062WatchdogState();
    s.armed = true;
    s.armed_timeout_ms = timeout_ms;
}

void Watchdog::feed() FL_NOEXCEPT {
    // 32-bit refresh key. Save+restore PRIMASK so we don't accidentally
    // re-enable interrupts when feed() is called from a context that had
    // them disabled (e.g. an ISR). __disable_irq()/__enable_irq() would
    // unconditionally re-enable, which is unsafe in nested-IRQ-off paths.
    fl::u32 primask;
    __asm__ volatile ("mrs %0, primask" : "=r" (primask));
    __asm__ volatile ("cpsid i" ::: "memory");
    WDOG3_CNT = platforms::kRtwdogRefreshKey;
    __asm__ volatile ("msr primask, %0" :: "r" (primask) : "memory");
}

void Watchdog::disable() FL_NOEXCEPT {
    // RTWDOG can be re-configured with CS.EN=0 via the unlock sequence as
    // long as CS.UPDATE was 1 in the previous config (which our begin()
    // always sets). Run the same unlock-then-CS pattern as begin() but
    // write CS with EN=0.
    auto& s = platforms::mxrt1062WatchdogState();
    if (!s.armed) return;

    // Save+restore PRIMASK so callers that already had IRQs off aren't
    // surprised by them coming back on.
    fl::u32 primask;
    __asm__ volatile ("mrs %0, primask" : "=r" (primask));
    __asm__ volatile ("cpsid i" ::: "memory");

    WDOG3_CNT = platforms::kRtwdogUnlockKey;
    {
        fl::u32 spin = 0;
        while (!(WDOG3_CS & WDOG_CS_ULK)) {
            if (++spin > 1000000u) {
                __asm__ volatile ("msr primask, %0" :: "r" (primask) : "memory");
                return;
            }
        }
    }

    WDOG3_TOVAL = 0xFFFFu;
    WDOG3_WIN   = 0;
    // EN = 0 → disable. Keep CMD32EN + UPDATE so a future begin() can re-arm.
    WDOG3_CS = WDOG_CS_CMD32EN | WDOG_CS_UPDATE | WDOG_CS_CLK(1);

    {
        fl::u32 spin = 0;
        while (!(WDOG3_CS & WDOG_CS_RCS)) {
            if (++spin > 1000000u) break;
        }
    }

    __asm__ volatile ("msr primask, %0" :: "r" (primask) : "memory");

    s.armed = false;
    s.armed_timeout_ms = 0;
}

ResetCause Watchdog::lastResetCause() const FL_NOEXCEPT {
    auto& s = platforms::mxrt1062WatchdogState();
    if (!s.cause_cached) {
        s.cached_cause = platforms::translateSrcSrsr(SRC_SRSR);
        // SRSR bits are sticky across resets — write-1-to-clear so the next
        // boot only sees its own cause.
        SRC_SRSR = SRC_SRSR;
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
