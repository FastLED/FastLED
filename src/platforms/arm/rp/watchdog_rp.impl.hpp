// SPDX-License-Identifier: LicenseRef-FastLED-Reciprocal-1.0
// AI LICENSE: LICENSE-AI-AGENT-INSTRUCTIONS.md
// AI agents must read that file before substantial FastLED changes.
// Substantial AI changes must be reported upstream with a reproducible patch.

// IWYU pragma: private

/// @file platforms/arm/rp/watchdog_rp.impl.hpp
/// @brief RP2040 / RP2350 watchdog implementation.
///
/// Pico SDK `hardware/watchdog.h` for begin/feed/disable. A timer expiry is
/// distinguished from the SDK/bootrom's intentional watchdog-based reboot
/// path with `watchdog_enable_caused_reboot()` on Pico SDK 1.3+. Older Pico
/// SDKs fall back to `watchdog_caused_reboot()`.
///
/// **Persist storage** uses the low 14 bytes of `watchdog_hw->scratch[0..3]`.
/// The upper two bytes of `scratch[3]` hold FastLED's consecutive watchdog
/// reset count; the SDK reserves `scratch[4..7]` for bootrom magic /
/// `reset_usb_boot()`.
/// Survives soft resets, NOT power cycles. RP2350 adds VBAT-backed
/// `POWMAN.SCRATCH[0..7]` which Phase 3 can use for power-cycle survival.
///
/// `rebootIntoBootloader()` calls `reset_usb_boot()` — the bootrom-supported
/// path to UF2 mode without touching BOOTSEL pin physically.

#include "fl/wdt/watchdog.h"
#include "fl/stl/singleton.h"
#include "platforms/arm/rp/is_rp.h"

// IWYU pragma: begin_keep
#include <hardware/watchdog.h>   // ok include — Pico SDK
#include <hardware/structs/watchdog.h>  // ok include — scratch[0..7]
#include <pico/bootrom.h>        // ok include — reset_usb_boot()
#include <pico/version.h>        // ok include — watchdog API capability guard
// IWYU pragma: end_keep

#define FL_WATCHDOG_HAS_HARDWARE
#define FL_WATCHDOG_PERSIST_BYTES 14   // scratch[0..2] + low half of scratch[3]
#define FL_WATCHDOG_HAS_BOOTLOADER_REBOOT

#if defined(PICO_SDK_VERSION_MAJOR) && defined(PICO_SDK_VERSION_MINOR) \
    && (PICO_SDK_VERSION_MAJOR > 1 \
        || (PICO_SDK_VERSION_MAJOR == 1 && PICO_SDK_VERSION_MINOR >= 3))
#define FL_WATCHDOG_HAS_RP_ENABLE_MARKER
#endif

#if defined(PICO_SDK_VERSION_MAJOR) && PICO_SDK_VERSION_MAJOR >= 2
#define FL_WATCHDOG_HAS_RP_DISABLE_API
#endif

#if defined(FL_IS_RP2350)
#define FL_WATCHDOG_MAX_TIMEOUT_MS 16777u
#else
#define FL_WATCHDOG_MAX_TIMEOUT_MS 8388u
#endif

namespace fl {
namespace platforms {

struct RpWatchdogState {
    bool       armed = false;
    ResetCause cached_cause = ResetCause::UNKNOWN;
    bool       cause_cached = false;
};

inline RpWatchdogState& rpWatchdogState() {
    return fl::Singleton<RpWatchdogState>::instance();
}

inline fl::u16 rpCrashCount() {
    return static_cast<fl::u16>(watchdog_hw->scratch[3] >> 16);
}

inline void rpSetCrashCount(fl::u16 count) {
    // Preserve bytes 12 and 13, which are the final public persist bytes.
    watchdog_hw->scratch[3] = (watchdog_hw->scratch[3] & 0x0000FFFFu)
        | (static_cast<fl::u32>(count) << 16);
}

inline ResetCause rpDetectResetCause() {
    // reset_usb_boot(), watchdog_reboot(), and UF2 flashing also use the
    // watchdog reset hardware. The SDK's enable marker survives only a real
    // timeout after watchdog_enable(); intentional bootloader/deploy resets
    // clear it. Counting every watchdog-caused reboot as a crash makes a few
    // normal deploys trip the retained safe-mode threshold and strand USB RPC.
#if defined(FL_WATCHDOG_HAS_RP_ENABLE_MARKER)
    if (watchdog_enable_caused_reboot()) return ResetCause::WATCHDOG;
    // On RP2350 the SDK deliberately makes watchdog_caused_reboot() false
    // for non-normal boot types, including reset_usb_boot(). The raw reason
    // bit is still set, and the missing enable marker above proves this was
    // an intentional watchdog-backed software reset rather than our timeout.
    if (watchdog_hw->reason) return ResetCause::SOFTWARE;
#else
    // Pico SDK 1.x (used by Arduino Mbed RP2040) predates the enable-marker
    // API. Preserve its supported watchdog detection even though that SDK
    // cannot distinguish a timeout from an intentional watchdog-backed reset.
    if (watchdog_caused_reboot()) return ResetCause::WATCHDOG;
#endif
    // Default to POWER_ON for first-boot heuristics.
    return ResetCause::POWER_ON;
}

} // namespace platforms

Watchdog& Watchdog::instance() FL_NO_EXCEPT {
    static Watchdog sInstance;
    return sInstance;
}

void Watchdog::begin(fl::u32 timeout_ms) FL_NO_EXCEPT {
    // Capture the previous boot's marker before watchdog_enable() writes the
    // current boot's enable marker into scratch[4]. Reading it afterward
    // makes every intentional SDK/bootrom reset look like a timer expiry.
    (void)lastResetCause();
    if (timeout_ms == 0) timeout_ms = 1000;
    if (timeout_ms > FL_WATCHDOG_MAX_TIMEOUT_MS) timeout_ms = FL_WATCHDOG_MAX_TIMEOUT_MS;
    // Second arg `pause_on_debug` defaults to true — useful in dev so a
    // debugger session doesn't reset the chip during a breakpoint.
    watchdog_enable(timeout_ms, true);
    platforms::rpWatchdogState().armed = true;
}

void Watchdog::feed() FL_NO_EXCEPT {
    watchdog_update();
}

void Watchdog::disable() FL_NO_EXCEPT {
    // Pico SDK's `watchdog_disable()` clears WATCHDOG_CTRL_ENABLE_BITS, which
    // halts the counter. Subsequent `feed()` calls are still safe (they only
    // write LOAD).
#if defined(FL_WATCHDOG_HAS_RP_DISABLE_API)
    watchdog_disable();
#else
    // Pico SDK 1.x has no watchdog_disable() helper. Its hardware struct and
    // atomic register helper expose the same operation used by the 2.x API.
    hw_clear_bits(&watchdog_hw->ctrl, WATCHDOG_CTRL_ENABLE_BITS);
#endif
    platforms::rpWatchdogState().armed = false;
}

ResetCause Watchdog::lastResetCause() const FL_NO_EXCEPT {
    auto& s = platforms::rpWatchdogState();
    if (!s.cause_cached) {
        s.cached_cause = platforms::rpDetectResetCause();
        s.cause_cached = true;
        if (s.cached_cause == ResetCause::WATCHDOG) {
            fl::u16 crash_count = platforms::rpCrashCount();
            if (crash_count < 0xFFFFu) {
                platforms::rpSetCrashCount(static_cast<fl::u16>(crash_count + 1));
            }
        } else {
            // A non-watchdog reset breaks a crash loop, matching the former
            // RAM-backed behavior while retaining the WDT count across reset.
            platforms::rpSetCrashCount(0);
        }
    }
    return s.cached_cause;
}

bool Watchdog::lastResetWasWatchdog() const FL_NO_EXCEPT {
    return lastResetCause() == ResetCause::WATCHDOG;
}

fl::u8 Watchdog::persistRead(fl::size idx) const FL_NO_EXCEPT {
    if (idx >= FL_WATCHDOG_PERSIST_BYTES) return 0;
    // Pack 4 bytes per scratch register, little-endian.
    fl::u32 word = watchdog_hw->scratch[idx >> 2];
    return static_cast<fl::u8>((word >> ((idx & 0x3) * 8)) & 0xFF);
}

void Watchdog::persistWrite(fl::size idx, fl::u8 v) FL_NO_EXCEPT {
    if (idx >= FL_WATCHDOG_PERSIST_BYTES) return;
    fl::u32 mask = static_cast<fl::u32>(0xFFu) << ((idx & 0x3) * 8);
    fl::u32 word = watchdog_hw->scratch[idx >> 2] & ~mask;
    word |= static_cast<fl::u32>(v) << ((idx & 0x3) * 8);
    watchdog_hw->scratch[idx >> 2] = word;
}

fl::u16 Watchdog::consecutiveCrashCount() const FL_NO_EXCEPT {
    (void)lastResetCause();
    return platforms::rpCrashCount();
}
void Watchdog::markCleanShutdown() FL_NO_EXCEPT { platforms::rpSetCrashCount(0); }
bool Watchdog::isInSafeMode() const FL_NO_EXCEPT { return consecutiveCrashCount() >= mSafeModeThreshold; }
fl::u16 Watchdog::safeModeThreshold() const FL_NO_EXCEPT { return mSafeModeThreshold; }
void    Watchdog::setSafeModeThreshold(fl::u16 t) FL_NO_EXCEPT { mSafeModeThreshold = t; }

FL_NO_RETURN void Watchdog::reboot() FL_NO_EXCEPT {
    watchdog_reboot(0, 0, 0);  // pc=0, sp=0, delay=0 — immediate
    while (true) {}
}

bool Watchdog::onTimeout(WatchdogTimeoutCallback, void*) FL_NO_EXCEPT { return false; }
bool Watchdog::onTimeout(fl::function<void()>) FL_NO_EXCEPT { return false; }
bool Watchdog::setPauseOnDebug(bool) FL_NO_EXCEPT {
    // Already set on begin() — caller would need to re-begin() to change it.
    return false;
}
bool Watchdog::writeCrashLog(fl::span<const fl::u8>) FL_NO_EXCEPT { return false; }
fl::size Watchdog::readCrashLog(fl::span<fl::u8>) const FL_NO_EXCEPT { return 0; }

bool Watchdog::rebootIntoBootloader() FL_NO_EXCEPT {
    // Force BOOTSEL/UF2 mode. Arg 0 = no LED activity GPIO mask, arg 0 =
    // both USB MSC and PICOBOOT interfaces enabled.
    reset_usb_boot(0, 0);
    return true;  // Should be unreachable, but contract is satisfied.
}

bool Watchdog::setWindow(fl::u32, fl::u32) FL_NO_EXCEPT { return false; }
bool Watchdog::hasCrashReport() const FL_NO_EXCEPT { return false; }
WatchdogCrashReport Watchdog::readCrashReport() const FL_NO_EXCEPT {
    WatchdogCrashReport r{}; r.valid = false; r.fault_type = ""; return r;
}
void Watchdog::clearCrashReport() FL_NO_EXCEPT {}

} // namespace fl

#undef FL_WATCHDOG_HAS_RP_ENABLE_MARKER
#undef FL_WATCHDOG_HAS_RP_DISABLE_API
