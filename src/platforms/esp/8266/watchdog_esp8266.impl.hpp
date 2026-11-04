// IWYU pragma: private

/// @file platforms/esp/8266/watchdog_esp8266.impl.hpp
/// @brief ESP8266 watchdog implementation on the NONOS SDK.
///
/// The chip's own watchdogs cannot be reconfigured: the core's software WDT
/// (~3.2 s, fed by yield()/loop) and the hardware WDT (~8 s) already reset a
/// sketch that stops yielding. fl::Watchdog adds the user-defined timeout on
/// top: an SDK os_timer checks the last feed() every 100 ms and restarts the
/// chip when it is older than the timeout. SDK timers only run when the
/// sketch returns to the SDK (between loop() passes, or yield()/delay()), so
/// the user timeout catches a sketch that keeps yielding without feeding;
/// one that stops yielding is reset by the core's software WDT instead. Either path reports
/// ResetCause::WATCHDOG on the next boot (rst_info reasons WDT/SOFT_WDT, or
/// a marker this file leaves in RTC memory before system_restart()).
///
/// **Persist storage** lives in RTC user memory, which survives soft resets
/// and deep sleep but not power loss. Blocks 0..31 are left to the core's
/// eboot/OTA command; FastLED uses blocks 96..100.

#include "fl/wdt/watchdog.h"
#include "fl/stl/chrono.h"
#include "fl/stl/singleton.h"
#include "fl/stl/noexcept.h"
#include "fl/stl/stdint.h"

// IWYU pragma: begin_keep
#include <osapi.h>           // SDK os_timer_*
#include <user_interface.h>  // SDK rst_info, RTC user memory, restart
// IWYU pragma: end_keep

#define FL_WATCHDOG_HAS_HARDWARE
#define FL_WATCHDOG_PERSIST_BYTES 14

namespace fl {
namespace platforms {

constexpr fl::u32 kEsp8266WdtMagic = 0x464C5744u;  // "FLWD"
constexpr fl::u32 kEsp8266WdtRtcBlock = 96;         // 4-byte blocks
constexpr fl::u32 kEsp8266WdtPollMs = 100;

struct Esp8266WdtRtc {
    fl::u32 magic;
    fl::u8  persist[FL_WATCHDOG_PERSIST_BYTES];
    fl::u16 crash_count;
    fl::u32 restart_by_watchdog;  // set just before our own restart
};

struct Esp8266WatchdogState {
    os_timer_t timer;
    fl::u32    timeout_ms = 0;
    volatile fl::u32 last_feed_ms = 0;
    bool       armed = false;
    bool       timer_set = false;
    bool       rtc_loaded = false;
    Esp8266WdtRtc rtc = {};
    ResetCause cached_cause = ResetCause::UNKNOWN;
    bool       cause_cached = false;
};

inline Esp8266WatchdogState& esp8266WdtState() FL_NO_EXCEPT {
    return fl::Singleton<Esp8266WatchdogState>::instance();
}

inline void esp8266WdtSave(Esp8266WatchdogState& s) FL_NO_EXCEPT {
    system_rtc_mem_write(kEsp8266WdtRtcBlock, &s.rtc, sizeof(s.rtc));
}

inline Esp8266WdtRtc& esp8266WdtRtc() FL_NO_EXCEPT {
    auto& s = esp8266WdtState();
    if (!s.rtc_loaded) {
        s.rtc_loaded = true;
        system_rtc_mem_read(kEsp8266WdtRtcBlock, &s.rtc, sizeof(s.rtc));
        if (s.rtc.magic != kEsp8266WdtMagic) {
            s.rtc = {};
            s.rtc.magic = kEsp8266WdtMagic;
            esp8266WdtSave(s);
        }
    }
    return s.rtc;
}

inline void esp8266WdtCheck(void*) FL_NO_EXCEPT {
    auto& s = esp8266WdtState();
    if (!s.armed) return;
    if (fl::millis() - s.last_feed_ms >= s.timeout_ms) {
        esp8266WdtRtc().restart_by_watchdog = 1;
        esp8266WdtSave(s);
        system_restart();
    }
}

inline ResetCause esp8266DetectResetCause() FL_NO_EXCEPT {
    auto& rtc = esp8266WdtRtc();
    const bool ours = rtc.restart_by_watchdog != 0;
    rtc.restart_by_watchdog = 0;
    esp8266WdtSave(esp8266WdtState());
    const rst_info* info = system_get_rst_info();
    switch (info ? info->reason : REASON_DEFAULT_RST) {
        case REASON_WDT_RST:
        case REASON_SOFT_WDT_RST: return ResetCause::WATCHDOG;
        case REASON_EXCEPTION_RST: return ResetCause::PANIC;
        case REASON_SOFT_RESTART:
            return ours ? ResetCause::WATCHDOG : ResetCause::SOFTWARE;
        case REASON_EXT_SYS_RST: return ResetCause::EXTERNAL_PIN;
        case REASON_DEFAULT_RST: return ResetCause::POWER_ON;
        default: return ResetCause::UNKNOWN;
    }
}

} // namespace platforms

Watchdog& Watchdog::instance() FL_NO_EXCEPT {
    static Watchdog sInstance;
    return sInstance;
}

void Watchdog::begin(fl::u32 timeout_ms) FL_NO_EXCEPT {
    (void)lastResetCause();
    auto& s = platforms::esp8266WdtState();
    s.timeout_ms = timeout_ms ? timeout_ms : 1000;
    s.last_feed_ms = fl::millis();
    if (!s.timer_set) {
        s.timer_set = true;
        os_timer_setfn(&s.timer, platforms::esp8266WdtCheck, nullptr);
    }
    os_timer_disarm(&s.timer);
    os_timer_arm(&s.timer, platforms::kEsp8266WdtPollMs, true);
    s.armed = true;
}

void Watchdog::feed() FL_NO_EXCEPT {
    platforms::esp8266WdtState().last_feed_ms = fl::millis();
    system_soft_wdt_feed();
}

void Watchdog::disable() FL_NO_EXCEPT {
    auto& s = platforms::esp8266WdtState();
    s.armed = false;
    if (s.timer_set) os_timer_disarm(&s.timer);
}

ResetCause Watchdog::lastResetCause() const FL_NO_EXCEPT {
    auto& s = platforms::esp8266WdtState();
    if (!s.cause_cached) {
        s.cached_cause = platforms::esp8266DetectResetCause();
        s.cause_cached = true;
        auto& rtc = platforms::esp8266WdtRtc();
        if (s.cached_cause == ResetCause::WATCHDOG ||
            s.cached_cause == ResetCause::PANIC) {
            if (rtc.crash_count < 0xFFFFu) ++rtc.crash_count;
        } else {
            rtc.crash_count = 0;
        }
        platforms::esp8266WdtSave(s);
    }
    return s.cached_cause;
}

bool Watchdog::lastResetWasWatchdog() const FL_NO_EXCEPT {
    return lastResetCause() == ResetCause::WATCHDOG;
}

fl::u8 Watchdog::persistRead(fl::size idx) const FL_NO_EXCEPT {
    if (idx >= FL_WATCHDOG_PERSIST_BYTES) return 0;
    return platforms::esp8266WdtRtc().persist[idx];
}

void Watchdog::persistWrite(fl::size idx, fl::u8 v) FL_NO_EXCEPT {
    if (idx >= FL_WATCHDOG_PERSIST_BYTES) return;
    platforms::esp8266WdtRtc().persist[idx] = v;
    platforms::esp8266WdtSave(platforms::esp8266WdtState());
}

fl::u16 Watchdog::consecutiveCrashCount() const FL_NO_EXCEPT {
    (void)lastResetCause();
    return platforms::esp8266WdtRtc().crash_count;
}
void Watchdog::markCleanShutdown() FL_NO_EXCEPT {
    platforms::esp8266WdtRtc().crash_count = 0;
    platforms::esp8266WdtSave(platforms::esp8266WdtState());
}
bool Watchdog::isInSafeMode() const FL_NO_EXCEPT { return consecutiveCrashCount() >= mSafeModeThreshold; }
fl::u16 Watchdog::safeModeThreshold() const FL_NO_EXCEPT { return mSafeModeThreshold; }
void    Watchdog::setSafeModeThreshold(fl::u16 t) FL_NO_EXCEPT { mSafeModeThreshold = t; }

FL_NO_RETURN void Watchdog::reboot() FL_NO_EXCEPT {
    system_restart();
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
