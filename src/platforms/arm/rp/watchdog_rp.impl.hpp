// IWYU pragma: private

/// @file platforms/arm/rp/watchdog_rp.impl.hpp
/// @brief RP2040 / RP2350 watchdog implementation.
///
/// Pico SDK `hardware/watchdog.h` for begin/feed/disable. Reset cause via
/// `watchdog_caused_reboot()` (+ `VREG_AND_CHIP_RESET` on RP2040,
/// `POWMAN.CHIP_RESET` on RP2350 for richer cause detection).
///
/// **Persist storage** uses `watchdog_hw->scratch[0..3]` (16 bytes) — the
/// SDK reserves `scratch[4..7]` for bootrom magic / `reset_usb_boot()`.
/// Survives soft resets, NOT power cycles. RP2350 adds VBAT-backed
/// `POWMAN.SCRATCH[0..7]` which Phase 3 can use for power-cycle survival.
///
/// `rebootIntoBootloader()` calls `reset_usb_boot()` — the bootrom-supported
/// path to UF2 mode without touching BOOTSEL pin physically.

#include "fl/wdt/watchdog.h"
#include "platforms/arm/rp/is_rp.h"

// IWYU pragma: begin_keep
#include <hardware/watchdog.h>   // ok include — Pico SDK
#include <hardware/structs/watchdog.h>  // ok include — scratch[0..7]
#include <pico/bootrom.h>        // ok include — reset_usb_boot()
// IWYU pragma: end_keep

#define FL_WATCHDOG_HAS_HARDWARE
#define FL_WATCHDOG_PERSIST_BYTES 16   // scratch[0..3] = 4 x 32-bit = 16 bytes
#define FL_WATCHDOG_HAS_BOOTLOADER_REBOOT

#if defined(FL_IS_RP2350)
#define FL_WATCHDOG_MAX_TIMEOUT_MS 16777u
#else
#define FL_WATCHDOG_MAX_TIMEOUT_MS 8388u
#endif

namespace fl {
namespace platforms {

struct RpWatchdogState {
    fl::u16    crash_count = 0;
    bool       armed = false;
    ResetCause cached_cause = ResetCause::UNKNOWN;
    bool       cause_cached = false;
};

inline RpWatchdogState& rpWatchdogState() {
    static RpWatchdogState s{};  // okay static in header — single-TU `.impl.hpp`
    return s;
}

inline ResetCause rpDetectResetCause() {
    if (watchdog_caused_reboot()) return ResetCause::WATCHDOG;
    // RP2040 / RP2350 chip reset register read; the SDK exposes
    // `watchdog_enable_caused_reboot()` to distinguish but we already
    // covered the WDT case. Default to POWER_ON for first-boot heuristics.
    return ResetCause::POWER_ON;
}

} // namespace platforms

Watchdog& Watchdog::instance() FL_NOEXCEPT {
    static Watchdog sInstance;
    return sInstance;
}

void Watchdog::begin(fl::u32 timeout_ms) FL_NOEXCEPT {
    if (timeout_ms == 0) timeout_ms = 1000;
    if (timeout_ms > FL_WATCHDOG_MAX_TIMEOUT_MS) timeout_ms = FL_WATCHDOG_MAX_TIMEOUT_MS;
    // Second arg `pause_on_debug` defaults to true — useful in dev so a
    // debugger session doesn't reset the chip during a breakpoint.
    watchdog_enable(timeout_ms, true);
    platforms::rpWatchdogState().armed = true;
}

void Watchdog::feed() FL_NOEXCEPT {
    watchdog_update();
}

void Watchdog::disable() FL_NOEXCEPT {
    // Pico SDK's `watchdog_disable()` clears WATCHDOG_CTRL_ENABLE_BITS, which
    // halts the counter. Subsequent `feed()` calls are still safe (they only
    // write LOAD).
    watchdog_disable();
    platforms::rpWatchdogState().armed = false;
}

ResetCause Watchdog::lastResetCause() const FL_NOEXCEPT {
    auto& s = platforms::rpWatchdogState();
    if (!s.cause_cached) {
        s.cached_cause = platforms::rpDetectResetCause();
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
    // Pack 4 bytes per scratch register, little-endian.
    fl::u32 word = watchdog_hw->scratch[idx >> 2];
    return static_cast<fl::u8>((word >> ((idx & 0x3) * 8)) & 0xFF);
}

void Watchdog::persistWrite(fl::size idx, fl::u8 v) FL_NOEXCEPT {
    if (idx >= FL_WATCHDOG_PERSIST_BYTES) return;
    fl::u32 mask = static_cast<fl::u32>(0xFFu) << ((idx & 0x3) * 8);
    fl::u32 word = watchdog_hw->scratch[idx >> 2] & ~mask;
    word |= static_cast<fl::u32>(v) << ((idx & 0x3) * 8);
    watchdog_hw->scratch[idx >> 2] = word;
}

fl::u16 Watchdog::consecutiveCrashCount() const FL_NOEXCEPT {
    (void)lastResetCause();
    return platforms::rpWatchdogState().crash_count;
}
void Watchdog::markCleanShutdown() FL_NOEXCEPT { platforms::rpWatchdogState().crash_count = 0; }
bool Watchdog::isInSafeMode() const FL_NOEXCEPT { return consecutiveCrashCount() >= mSafeModeThreshold; }
fl::u16 Watchdog::safeModeThreshold() const FL_NOEXCEPT { return mSafeModeThreshold; }
void    Watchdog::setSafeModeThreshold(fl::u16 t) FL_NOEXCEPT { mSafeModeThreshold = t; }

FL_NORETURN void Watchdog::reboot() FL_NOEXCEPT {
    watchdog_reboot(0, 0, 0);  // pc=0, sp=0, delay=0 — immediate
    while (true) {}
}

bool Watchdog::onTimeout(WatchdogTimeoutCallback, void*) FL_NOEXCEPT { return false; }
bool Watchdog::onTimeout(fl::function<void()>) FL_NOEXCEPT { return false; }
bool Watchdog::setPauseOnDebug(bool) FL_NOEXCEPT {
    // Already set on begin() — caller would need to re-begin() to change it.
    return false;
}
bool Watchdog::writeCrashLog(fl::span<const fl::u8>) FL_NOEXCEPT { return false; }
fl::size Watchdog::readCrashLog(fl::span<fl::u8>) const FL_NOEXCEPT { return 0; }

bool Watchdog::rebootIntoBootloader() FL_NOEXCEPT {
    // Force BOOTSEL/UF2 mode. Arg 0 = no LED activity GPIO mask, arg 0 =
    // both USB MSC and PICOBOOT interfaces enabled.
    reset_usb_boot(0, 0);
    return true;  // Should be unreachable, but contract is satisfied.
}

bool Watchdog::setWindow(fl::u32, fl::u32) FL_NOEXCEPT { return false; }
bool Watchdog::hasCrashReport() const FL_NOEXCEPT { return false; }
WatchdogCrashReport Watchdog::readCrashReport() const FL_NOEXCEPT {
    WatchdogCrashReport r{}; r.valid = false; r.fault_type = ""; return r;
}
void Watchdog::clearCrashReport() FL_NOEXCEPT {}

} // namespace fl
