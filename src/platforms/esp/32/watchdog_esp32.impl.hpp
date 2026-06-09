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

#include "esp_idf_version.h"
#include "esp_system.h"   // esp_reset_reason()
#include "esp_task_wdt.h"
// IWYU pragma: begin_keep
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
// IWYU pragma: end_keep

#define FL_WATCHDOG_HAS_HARDWARE
#define FL_WATCHDOG_PERSIST_BYTES 16
#define FL_WATCHDOG_MAX_TIMEOUT_MS 60000u
#define FL_WATCHDOG_HAS_PRE_TIMEOUT_IRQ

namespace fl {
namespace platforms {

// Persist layout: user payload occupies bytes [0, kUserStart);
// the trailing 2 bytes of persist[] hold the consecutive crash_count.
// When persist[] moves to RTC_NOINIT_ATTR / __NOINIT_ATTR in Phase 2 (#2731),
// crash_count will then survive real resets automatically without further code
// change.
struct Esp32WatchdogState {
    static constexpr fl::size kCrashCountBytes = 2;
    static constexpr fl::size kUserBytes =
        FL_WATCHDOG_PERSIST_BYTES > kCrashCountBytes
            ? (FL_WATCHDOG_PERSIST_BYTES - kCrashCountBytes)
            : 0;

    fl::u8     persist[FL_WATCHDOG_PERSIST_BYTES] = {0};
    bool       armed = false;
    fl::u32    armed_timeout_ms = 0;
    void*      monitored_task = nullptr;  // TaskHandle_t; stored as void* to keep this state POD-like.
    bool       monitored_task_added = false;
    ResetCause cached_cause = ResetCause::UNKNOWN;
    bool       cause_cached = false;

    fl::u16 load_crash_count() const {
        // Little-endian read from the tail of persist[].
        return static_cast<fl::u16>(persist[kUserBytes])
             | (static_cast<fl::u16>(persist[kUserBytes + 1]) << 8);
    }
    void store_crash_count(fl::u16 v) {
        persist[kUserBytes]     = static_cast<fl::u8>(v & 0xFF);
        persist[kUserBytes + 1] = static_cast<fl::u8>((v >> 8) & 0xFF);
    }
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

inline TaskHandle_t esp32MonitoredTask() {
    return static_cast<TaskHandle_t>(esp32WatchdogState().monitored_task);
}

inline bool esp32SubscribeCurrentTask() {
    if (xTaskGetSchedulerState() != taskSCHEDULER_RUNNING) {
        return false;
    }

    TaskHandle_t current_task = xTaskGetCurrentTaskHandle();
    if (current_task == nullptr) {
        return false;
    }

    auto& s = esp32WatchdogState();
    if (s.monitored_task == current_task && s.monitored_task != nullptr) {
        (void)esp_task_wdt_reset();
        return true;
    }

    if (s.monitored_task != nullptr && s.monitored_task_added) {
        (void)esp_task_wdt_delete(esp32MonitoredTask());
    }
    s.monitored_task = nullptr;
    s.monitored_task_added = false;

    esp_err_t status = esp_task_wdt_status(current_task);
    if (status == ESP_OK) {
        s.monitored_task = current_task;
        s.monitored_task_added = false;
        (void)esp_task_wdt_reset();
        return true;
    }

    if (status == ESP_ERR_INVALID_STATE) {
        return false;
    }

    esp_err_t err = esp_task_wdt_add(current_task);
    if (err != ESP_OK) {
        return false;
    }

    s.monitored_task = current_task;
    s.monitored_task_added = true;
    (void)esp_task_wdt_reset();
    return true;
}

inline void esp32ResetMonitoredTask() {
    auto& s = esp32WatchdogState();
    if (!s.armed || s.monitored_task == nullptr) {
        return;
    }
    if (xTaskGetSchedulerState() != taskSCHEDULER_RUNNING) {
        return;
    }
    if (xTaskGetCurrentTaskHandle() != esp32MonitoredTask()) {
        return;
    }
    (void)esp_task_wdt_reset();
}

inline bool esp32UnsubscribeMonitoredTask() {
    auto& s = esp32WatchdogState();
    if (s.monitored_task != nullptr && s.monitored_task_added) {
        esp_err_t err = esp_task_wdt_delete(esp32MonitoredTask());
        if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
            return false;
        }
    }
    s.monitored_task = nullptr;
    s.monitored_task_added = false;
    return true;
}

} // namespace platforms

// `Watchdog::instance()` is defined here (and in every sibling `.impl.hpp` /
// the noop) so the function-local static lives in exactly one TU per program.
// Avoids the Teensy 3.x `__cxa_guard` ABI conflict that a header-side
// definition would impose on every TU including FastLED.h.
Watchdog& Watchdog::instance() FL_NOEXCEPT {
    static Watchdog sInstance;
    return sInstance;
}

void Watchdog::begin(fl::u32 timeout_ms) FL_NOEXCEPT {
    if (timeout_ms == 0) timeout_ms = 1000;
    if (timeout_ms > FL_WATCHDOG_MAX_TIMEOUT_MS) timeout_ms = FL_WATCHDOG_MAX_TIMEOUT_MS;
    auto& s = platforms::esp32WatchdogState();
    s.armed_timeout_ms = timeout_ms;
    fl::watchdog_setup(timeout_ms);
    (void)platforms::esp32SubscribeCurrentTask();
    s.armed = true;
}

void Watchdog::feed() FL_NOEXCEPT {
    platforms::esp32ResetMonitoredTask();
}

void Watchdog::disable() FL_NOEXCEPT {
    // Real TWDT teardown via `fl::watchdog_disable()` (wraps
    // `esp_task_wdt_deinit()` and is safely guarded against being called
    // before the FreeRTOS scheduler is running). Only clear the local armed
    // state if teardown actually succeeded — if `esp_task_wdt_deinit()`
    // returned non-OK (other tasks still subscribed, already deinitialized)
    // the TWDT is still active, so reporting "disabled" would lie. Leaving
    // `armed`/`armed_timeout_ms` intact also means a subsequent `begin()`
    // can reason about its prior state.
    auto& s = platforms::esp32WatchdogState();
    if (fl::watchdog_disable()) {
        (void)platforms::esp32UnsubscribeMonitoredTask();
        s.armed = false;
        s.armed_timeout_ms = 0;
    }
}

ResetCause Watchdog::lastResetCause() const FL_NOEXCEPT {
    auto& s = platforms::esp32WatchdogState();
    if (!s.cause_cached) {
        s.cached_cause = platforms::translateEsp32ResetReason(esp_reset_reason());
        s.cause_cached = true;
        if (s.cached_cause == ResetCause::WATCHDOG || s.cached_cause == ResetCause::PANIC) {
            fl::u16 cc = s.load_crash_count();
            if (cc < 0xFFFF) ++cc;
            s.store_crash_count(cc);
        }
    }
    return s.cached_cause;
}

bool Watchdog::lastResetWasWatchdog() const FL_NOEXCEPT {
    return lastResetCause() == ResetCause::WATCHDOG;
}

fl::u8 Watchdog::persistRead(fl::size idx) const FL_NOEXCEPT {
    // User-visible persist region is the leading kUserBytes; the trailing
    // bytes hold the serialized crash_count and are NOT exposed.
    if (idx >= platforms::Esp32WatchdogState::kUserBytes) return 0;
    return platforms::esp32WatchdogState().persist[idx];
}

void Watchdog::persistWrite(fl::size idx, fl::u8 v) FL_NOEXCEPT {
    if (idx >= platforms::Esp32WatchdogState::kUserBytes) return;
    platforms::esp32WatchdogState().persist[idx] = v;
}

fl::u16 Watchdog::consecutiveCrashCount() const FL_NOEXCEPT {
    // Side-effect: ensure cached_cause has bumped the counter at least once
    (void)lastResetCause();
    return platforms::esp32WatchdogState().load_crash_count();
}

void Watchdog::markCleanShutdown() FL_NOEXCEPT {
    platforms::esp32WatchdogState().store_crash_count(0);
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
    // Tier 1 hookup: re-init the watchdog with a callback bound. Reuse the
    // timeout that begin() armed earlier — registering a callback must not
    // silently shorten the watchdog window. If begin() hasn't been called,
    // fall back to the documented default (5000 ms).
    auto& s = platforms::esp32WatchdogState();
    fl::u32 timeout_ms = s.armed_timeout_ms != 0 ? s.armed_timeout_ms : 5000u;
    fl::watchdog_setup(timeout_ms, cb, user_data);
    (void)platforms::esp32SubscribeCurrentTask();
    s.armed = true;
    s.armed_timeout_ms = timeout_ms;
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
