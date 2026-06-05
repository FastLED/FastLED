// IWYU pragma: private

/// @file platforms/avr/watchdog_avr.impl.hpp
/// @brief AVR watchdog implementation (ATmega328P/2560/ATtiny85 and friends).
///
/// Backed by `<avr/wdt.h>` and `MCUSR` for cause detection. Max timeout 8 s
/// (the hardware ceiling). Persist storage is in-RAM only; Phase 3 follow-up
/// can move it to AVR EEPROM (`<avr/eeprom.h>`) with appropriate wear-leveling.
///
/// **CRITICAL — `WDRF` boot-loop fix:** After a watchdog reset, `WDRF` stays
/// set in `MCUSR` and the WDT stays armed at the minimum interval (16 ms).
/// If the sketch doesn't `wdt_disable()` before that 16 ms passes, the board
/// loops forever. Optiboot clears this; older bootloaders do NOT, and ATtiny
/// has no Optiboot. We install a `.init3` constructor that runs before
/// `main()`/`setup()` to clear `MCUSR` and disable the WDT, then capture the
/// reset cause for `lastResetCause()` to report.

#include "fl/wdt/watchdog.h"

// IWYU pragma: begin_keep
#include <avr/wdt.h>     // ok include — AVR-specific WDT API
#include <avr/io.h>      // ok include — MCUSR
// IWYU pragma: end_keep

// Capability gate. The implementation below assumes the classic-AVR symbol
// surface (`MCUSR` + `WDRF`/`BORF`/`EXTRF`/`PORF` reset-cause bits, and the
// extended `WDTO_8S` timeout constant). That surface is present on the bulk
// of the AVR line — ATmega328P, ATmega2560, ATtiny85, ATtiny88, ATtiny4313,
// etc. — but is **not** universal:
//
//   * **ATmega8 / ATmega8A** — the original ATmega8 caps the WDT prescaler
//     at `WDTO_2S`; `WDTO_4S` and `WDTO_8S` are not defined. Reset cause is
//     reported via `MCUCSR` (different name) on these older parts.
//   * **megaAVR-0 / tinyAVR-1 series** (ATmega4809 a.k.a. nano_every,
//     ATtiny1604, ATtiny1616, etc.) — these use the new RSTCTRL peripheral
//     for reset cause and the new WDT peripheral via `WDT.CTRLA`. None of
//     `MCUSR`, `WDRF`, `EXTRF`, `PORF`, or `WDTO_8S` are declared on these
//     chips. The avr-libc `<avr/wdt.h>` shim exists but the legacy symbols
//     do not.
//
// When the required symbols aren't all present we fall back to the platform-
// agnostic no-op watchdog so the firmware still links. Real hardware WDT
// support for the megaAVR-0 / tinyAVR-1 / ATmega8 families can be added
// later as separate per-family impls without changing the dispatcher.
#if !(defined(WDTO_8S) && defined(MCUSR) && defined(WDRF) && \
      defined(BORF) && defined(EXTRF) && defined(PORF))

#include "platforms/shared/watchdog_noop.hpp"

#else  // classic-AVR symbol surface available — full hardware implementation

#define FL_WATCHDOG_HAS_HARDWARE
#define FL_WATCHDOG_PERSIST_BYTES 8
#define FL_WATCHDOG_MAX_TIMEOUT_MS 8000u

namespace fl {
namespace platforms {

// Captured copy of MCUSR before we cleared it in .init3. Set in the
// constructor below, read by lastResetCause().
//
// CRITICAL: This MUST live in `.noinit` (no initializer, no `= 0`). avr-libc's
// `.init4` step clears `.bss` and copies `.data`, which runs AFTER our `.init3`
// hook. Any `.bss`/`.data` placement would clobber the captured MCUSR before
// `lastResetCause()` could read it. `.noinit` is excluded from that step.
extern fl::u8 sAvrCapturedMcusr;
__attribute__((section(".noinit"))) fl::u8 sAvrCapturedMcusr;

struct AvrWatchdogState {
    fl::u8     persist[FL_WATCHDOG_PERSIST_BYTES] = {0};
    fl::u16    crash_count = 0;
    bool       armed = false;
    ResetCause cached_cause = ResetCause::UNKNOWN;
    bool       cause_cached = false;
};

inline AvrWatchdogState& avrWatchdogState() {
    static AvrWatchdogState s{};  // okay static in header — single-TU `.impl.hpp`
    return s;
}

// Translate AVR `MCUSR` bits to fl::ResetCause. Priority order matches the
// historical "specific first" reading of the register: WDRF wins over PORF
// because the latter is often co-set after a watchdog reset.
inline ResetCause translateAvrMcusr(fl::u8 mcusr) {
    if (mcusr & (1 << WDRF))  return ResetCause::WATCHDOG;
    if (mcusr & (1 << BORF))  return ResetCause::BROWNOUT;
    if (mcusr & (1 << EXTRF)) return ResetCause::EXTERNAL_PIN;
    if (mcusr & (1 << PORF))  return ResetCause::POWER_ON;
    return ResetCause::UNKNOWN;
}

// Convert a millisecond timeout to the closest `WDTO_*` constant. The AVR
// watchdog only supports a discrete set; pick the smallest constant whose
// timeout is >= the requested timeout (or the maximum if exceeded).
inline fl::u8 timeoutToWdtoConstant(fl::u32 ms) {
    // Use the actual AVR watchdog prescaler bucket boundaries:
    // WDTO_15MS=16ms, WDTO_30MS=32ms, WDTO_60MS=64ms, WDTO_120MS=125ms.
    if (ms <=   16) return WDTO_15MS;
    if (ms <=   32) return WDTO_30MS;
    if (ms <=   64) return WDTO_60MS;
    if (ms <=  125) return WDTO_120MS;
    if (ms <=  250) return WDTO_250MS;
    if (ms <=  500) return WDTO_500MS;
    if (ms <= 1000) return WDTO_1S;
    if (ms <= 2000) return WDTO_2S;
    if (ms <= 4000) return WDTO_4S;
    return WDTO_8S;
}

// `.init3` hook: runs before avr-libc's `.init4` BSS/data initialization
// and before `main()`. Captures MCUSR for later cause reporting, then clears
// it and disables the WDT so a post-WDT-reset boot cannot loop forever.
// (See the `sAvrCapturedMcusr` declaration above for why that variable must
// stay in `.noinit` — `.init4` would otherwise wipe it after we capture.)
//
// **MUST be `naked`.** `.init3` is not a function call site — the linker
// concatenates this code inline between `.init2` and `.init4`, and there is
// no return address on the stack when it runs. A non-`naked` function emits
// a prologue/epilogue ending in `ret`, which on AVR pops two bytes of
// uninitialized SRAM and jumps there, hanging the chip before `setup()` can
// run. This is the canonical avr-libc pattern for clearing `MCUSR` /
// `WDT` in `.init3`; see issue #2798 for the AVR8JS regression that proved
// out the previous non-naked version. The three statements below compile
// to bare `STS`/inline-asm sequences with no stack temps, so `naked` is
// safe — and is in fact the only correct choice for code placed in `.initN`.
__attribute__((naked, used, section(".init3")))
void fastled_watchdog_init3() {
    sAvrCapturedMcusr = MCUSR;
    MCUSR = 0;
    wdt_disable();
    // Intentionally no `ret`: the linker concatenates `.init3` directly into
    // the boot init chain and execution falls through to `.init4`.
}

} // namespace platforms

// `Watchdog::instance()` is defined here so the function-local static lives in
// exactly one TU per program (avoiding Teensy 3.x `__cxa_guard` ABI conflicts —
// not strictly an issue on AVR but kept for consistency with sibling impls).
Watchdog& Watchdog::instance() FL_NOEXCEPT {
    static Watchdog sInstance;
    return sInstance;
}

void Watchdog::begin(fl::u32 timeout_ms) FL_NOEXCEPT {
    if (timeout_ms == 0) timeout_ms = 1000;
    if (timeout_ms > FL_WATCHDOG_MAX_TIMEOUT_MS) timeout_ms = FL_WATCHDOG_MAX_TIMEOUT_MS;
    wdt_enable(platforms::timeoutToWdtoConstant(timeout_ms));
    platforms::avrWatchdogState().armed = true;
}

void Watchdog::feed() FL_NOEXCEPT {
    wdt_reset();
}

void Watchdog::disable() FL_NOEXCEPT {
    wdt_disable();
    platforms::avrWatchdogState().armed = false;
}

ResetCause Watchdog::lastResetCause() const FL_NOEXCEPT {
    auto& s = platforms::avrWatchdogState();
    if (!s.cause_cached) {
        s.cached_cause = platforms::translateAvrMcusr(platforms::sAvrCapturedMcusr);
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
    return platforms::avrWatchdogState().persist[idx];
}

void Watchdog::persistWrite(fl::size idx, fl::u8 v) FL_NOEXCEPT {
    if (idx >= FL_WATCHDOG_PERSIST_BYTES) return;
    platforms::avrWatchdogState().persist[idx] = v;
}

fl::u16 Watchdog::consecutiveCrashCount() const FL_NOEXCEPT {
    (void)lastResetCause();
    return platforms::avrWatchdogState().crash_count;
}

void Watchdog::markCleanShutdown() FL_NOEXCEPT {
    platforms::avrWatchdogState().crash_count = 0;
}

bool Watchdog::isInSafeMode() const FL_NOEXCEPT {
    return consecutiveCrashCount() >= mSafeModeThreshold;
}

fl::u16 Watchdog::safeModeThreshold() const FL_NOEXCEPT { return mSafeModeThreshold; }
void    Watchdog::setSafeModeThreshold(fl::u16 t) FL_NOEXCEPT { mSafeModeThreshold = t; }

FL_NORETURN void Watchdog::reboot() FL_NOEXCEPT {
    // Force a watchdog-driven software reset.
    wdt_enable(WDTO_15MS);
    while (true) {}
}

// Tier 1/2 — not implemented on AVR (no pre-timeout IRQ wired, no flash
// crash log, no bootloader-reboot path that wouldn't risk bricking).
bool Watchdog::onTimeout(WatchdogTimeoutCallback, void*) FL_NOEXCEPT { return false; }
bool Watchdog::onTimeout(fl::function<void()>) FL_NOEXCEPT { return false; }
bool Watchdog::setPauseOnDebug(bool) FL_NOEXCEPT { return false; }
bool Watchdog::writeCrashLog(fl::span<const fl::u8>) FL_NOEXCEPT { return false; }
fl::size Watchdog::readCrashLog(fl::span<fl::u8>) const FL_NOEXCEPT { return 0; }
bool Watchdog::rebootIntoBootloader() FL_NOEXCEPT { return false; }

bool                Watchdog::setWindow(fl::u32, fl::u32) FL_NOEXCEPT { return false; }
bool                Watchdog::hasCrashReport() const FL_NOEXCEPT { return false; }
WatchdogCrashReport Watchdog::readCrashReport() const FL_NOEXCEPT {
    WatchdogCrashReport r{};
    r.valid = false;
    r.fault_type = "";
    return r;
}
void                Watchdog::clearCrashReport() FL_NOEXCEPT {}

} // namespace fl

#endif  // classic-AVR symbol surface available
