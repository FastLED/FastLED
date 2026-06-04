// IWYU pragma: private

/// @file platforms/esp/32/watchdog_esp32.impl.hpp
/// @brief ESP32 watchdog implementation for the unified fl::Watchdog API.
///
/// Wraps the existing `fl::watchdog_setup()` entry point (already implemented
/// in src/platforms/esp/32/watchdog_esp32.{h,cpp.hpp} and the IDF v4/v5
/// sub-impls). The existing weak-symbol panic override and USB-CDC disconnect
/// behavior — which fix the Windows phantom-COM-port bug — are preserved
/// verbatim.
///
/// **Tier 0 surface** is fully supported via the TWDT and `esp_reset_reason()`.
/// Persist storage falls back to in-RAM (NOT crash-resistant) for now — Phase 2
/// will move it to `RTC_NOINIT_ATTR` on variants that have RTC slow memory and
/// `__NOINIT_ATTR` on ESP32-P4 which has no RTC slow memory.
///
/// **Tier 1** — onTimeout(C-pointer) wires through to the existing
/// `watchdog_callback_t` slot. Other Tier 1 methods return false.
///
/// **Tier 2** — not implemented; tracked in #2731.

#include "fl/wdt/watchdog.h"
#include "platforms/esp/32/watchdog_esp32.h"

extern "C" {
#include "esp_system.h"   // esp_reset_reason()
#include "esp_idf_version.h"
}

#define FL_WATCHDOG_HAS_HARDWARE
#define FL_WATCHDOG_PERSIST_BYTES 16
#define FL_WATCHDOG_MAX_TIMEOUT_MS 60000u
#define FL_WATCHDOG_HAS_PRE_TIMEOUT_IRQ

namespace fl {
namespace platforms {

struct Esp32WatchdogState {
    fl::u8  persist[FL_WATCHDOG_PERSIST_BYTES] = {0};
    fl::u16 crash_count = 0;
    bool    armed = false;
    ResetCause cached_cause = ResetCause::UNKNOWN;
    bool    cause_cached = false;
};

inline Esp32WatchdogState& esp32WatchdogState() {
    static Esp32WatchdogState s{};  // okay static in header — this `.impl.hpp` is included from exactly one TU (the dispatcher)
    return s;
}

inline ResetCause translateEsp32ResetReason(esp_reset_reason_t r) {
    switch (r) {
        case ESP_RST_POWERON:  return ResetCause::POWER_ON;
        case ESP_RST_EXT:      return ResetCause::EXTERNAL_PIN;
        case ESP_RST_SW:       return ResetCause::SOFTWARE;
        case ESP_RST_PANIC:    return ResetCause::PANIC;
        case ESP_RST_INT_WDT:  return ResetCause::WATCHDOG;
        case ESP_RST_TASK_WDT: return ResetCause::WATCHDOG;
        case ESP_RST_WDT:      return ResetCause::WATCHDOG;
        case ESP_RST_BROWNOUT: return ResetCause::BROWNOUT;
        case ESP_RST_DEEPSLEEP:
        case ESP_RST_SDIO:
        default:               return ResetCause::UNKNOWN;
    }
}

} // namespace platforms

// `Watchdog::instance()` defined in fl/wdt/watchdog.h — every TU resolves it
// there. Rest of the API is defined non-inline here; this header is only
// included from the dispatcher in one TU.

void Watchdog::begin(fl::u32 timeout_ms) FL_NOEXCEPT {
    if (timeout_ms == 0) timeout_ms = 1000;
    if (timeout_ms > FL_WATCHDOG_MAX_TIMEOUT_MS) timeout_ms = FL_WATCHDOG_MAX_TIMEOUT_MS;
    auto& s = platforms::esp32WatchdogState();
    fl::watchdog_setup(timeout_ms);
    s.armed = true;
}

void Watchdog::feed() FL_NOEXCEPT {
    // ESP32 framework auto-feeds via idle-task monitoring (see watchdog_esp32_idf{4,5}.hpp).
    // No-op here; loop() liveness alone keeps the TWDT happy.
}

void Watchdog::disable() FL_NOEXCEPT {
    // No-op for now; TWDT deinit requires the FreeRTOS scheduler running and
    // careful sequencing. Tracked in #2731.
}

ResetCause Watchdog::lastResetCause() const FL_NOEXCEPT {
    auto& s = platforms::esp32WatchdogState();
    if (!s.cause_cached) {
        s.cached_cause = platforms::translateEsp32ResetReason(esp_reset_reason());
        s.cause_cached = true;
        if (s.cached_cause == ResetCause::WATCHDOG || s.cached_cause == ResetCause::PANIC) {
            if (s.crash_count < 0xFFFF) s.crash_count++;
        }
    }
    return s.cached_cause;
}

bool Watchdog::lastResetWasWatchdog() const FL_NOEXCEPT {
    return lastResetCause() == ResetCause::WATCHDOG;
}

fl::u8 Watchdog::persistRead(fl::size idx) const FL_NOEXCEPT {
    if (idx >= FL_WATCHDOG_PERSIST_BYTES) return 0;
    return platforms::esp32WatchdogState().persist[idx];
}

void Watchdog::persistWrite(fl::size idx, fl::u8 v) FL_NOEXCEPT {
    if (idx >= FL_WATCHDOG_PERSIST_BYTES) return;
    platforms::esp32WatchdogState().persist[idx] = v;
}

fl::u16 Watchdog::consecutiveCrashCount() const FL_NOEXCEPT {
    // Side-effect: ensure cached_cause has bumped the counter at least once
    (void)lastResetCause();
    return platforms::esp32WatchdogState().crash_count;
}

void Watchdog::markCleanShutdown() FL_NOEXCEPT {
    platforms::esp32WatchdogState().crash_count = 0;
}

bool Watchdog::isInSafeMode() const FL_NOEXCEPT {
    return consecutiveCrashCount() >= mSafeModeThreshold;
}

fl::u16 Watchdog::safeModeThreshold() const FL_NOEXCEPT { return mSafeModeThreshold; }
void    Watchdog::setSafeModeThreshold(fl::u16 t) FL_NOEXCEPT { mSafeModeThreshold = t; }

FL_NORETURN void Watchdog::reboot() FL_NOEXCEPT {
    esp_restart();
    while (true) {}
}

bool Watchdog::onTimeout(WatchdogTimeoutCallback cb, void* user_data) FL_NOEXCEPT {
    // Tier 1 hookup: re-init the watchdog with a callback bound. We don't
    // know the timeout the user passed earlier, so use the existing 5000 ms
    // default if begin() hasn't been called yet.
    fl::watchdog_setup(5000, cb, user_data);
    return true;
}

bool Watchdog::onTimeout(fl::function<void()> /*cb*/) FL_NOEXCEPT {
    // fl::function trampoline requires allocator-safe storage; deferred to
    // a follow-up PR. Tier 1 C-pointer path above remains usable.
    return false;
}

bool Watchdog::setPauseOnDebug(bool /*pause*/) FL_NOEXCEPT { return false; }
bool Watchdog::writeCrashLog(fl::span<const fl::u8> /*payload*/) FL_NOEXCEPT { return false; }
fl::size Watchdog::readCrashLog(fl::span<fl::u8> /*out*/) const FL_NOEXCEPT { return 0; }
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
