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
    if (timeout_ms == 0) timeout_ms = 1000;
    if (timeout_ms > FL_WATCHDOG_MAX_TIMEOUT_MS) timeout_ms = FL_WATCHDOG_MAX_TIMEOUT_MS;

    // Convert ms to TOVAL counts. With LPO (~32 kHz) and /256 prescaler
    // (CS.PRES set) the tick rate is 125 Hz → 8 ms per count. Clamp to the
    // 16-bit register range.
    fl::u32 toval = timeout_ms / 8u;
    if (toval == 0)         toval = 1;
    if (toval > 0xFFFFu)    toval = 0xFFFFu;

    __disable_irq();

    // 32-bit unlock key (atomic store; relies on CS.CMD32EN=1, which is the
    // RTWDOG reset default and which we restore in the CS write below).
    WDOG3_CNT = platforms::kRtwdogUnlockKey;
    // Wait for the unlock window to open. The RM allows up to ~32 LPO ticks;
    // at 32 kHz that's ~1 ms, far longer than the bus stores below take.
    while (!(WDOG3_CS & WDOG_CS_ULK)) { /* spin */ }

    WDOG3_TOVAL = toval;
    WDOG3_WIN   = 0;   // No window mode — feeds at any time are valid.

    // Configure CS:
    //  - EN        = 1  → enable RTWDOG
    //  - CMD32EN   = 1  → keep 32-bit unlock/refresh keying (matches the
    //                     CNT writes we use)
    //  - UPDATE    = 1  → allow future reconfigures via the unlock sequence
    //  - PRES      = 1  → /256 prescaler → 8 ms per TOVAL count
    //  - CLK = 0b01    → LPO clock source (32 kHz, low-power, runs in wait/stop)
    //  - WAIT/STOP/DBG → leave at reset defaults (continue counting in WAIT
    //                    and STOP; stop in DEBUG so a debugger session
    //                    doesn't reset the chip during a breakpoint).
    WDOG3_CS = WDOG_CS_EN
             | WDOG_CS_CMD32EN
             | WDOG_CS_UPDATE
             | WDOG_CS_PRES
             | WDOG_CS_CLK(1)
             | WDOG_CS_DBG;

    // Wait for the reconfigure to take effect.
    while (!(WDOG3_CS & WDOG_CS_RCS)) { /* spin */ }

    __enable_irq();

    auto& s = platforms::mxrt1062WatchdogState();
    s.armed = true;
    s.armed_timeout_ms = timeout_ms;
}

void Watchdog::feed() FL_NOEXCEPT {
    // 32-bit refresh key. Interrupts off so an ISR can't tear the write.
    __disable_irq();
    WDOG3_CNT = platforms::kRtwdogRefreshKey;
    __enable_irq();
}

void Watchdog::disable() FL_NOEXCEPT {
    // RTWDOG cannot be cleanly disabled once armed and locked. To honor the
    // Tier-0 contract conservatively, reconfigure with the maximum supported
    // timeout so calling code that documents "watchdog disabled" still has
    // the longest possible window before reset. Real teardown isn't possible
    // without violating the CS write-protect after the first config.
    auto& s = platforms::mxrt1062WatchdogState();
    if (!s.armed) return;
    begin(FL_WATCHDOG_MAX_TIMEOUT_MS);
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
