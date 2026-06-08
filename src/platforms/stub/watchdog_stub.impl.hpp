// IWYU pragma: private

/// @file platforms/stub/watchdog_stub.impl.hpp
/// @brief Host/stub watchdog implementation for unit tests.
///
/// Provides a fully working `fl::Watchdog` for the host build using a
/// thread-backed deadline timer. The stub never std::abort()s the test
/// process — host unit tests verify reset behavior by observing the callback
/// firing and the crash counter incrementing. Real-hardware process
/// termination is not simulated.
///
/// Persist storage is backed by a 16-byte in-process buffer that survives
/// across Watchdog::reboot()/feed() cycles within a single test process.
///
/// Capability macros defined here:
///   - FL_WATCHDOG_HAS_HARDWARE          (host build counts as "has WDT")
///   - FL_WATCHDOG_PERSIST_BYTES = 16
///   - FL_WATCHDOG_MAX_TIMEOUT_MS = 600000
///   - FL_WATCHDOG_HAS_PRE_TIMEOUT_IRQ   (callback fires from timer thread)
///   - FL_WATCHDOG_HAS_CRASH_REPORT       (synthesized — no real fault frame)
///
/// `FL_WATCHDOG_HAS_WINDOW_MODE` and `FL_WATCHDOG_HAS_BOOTLOADER_REBOOT` are
/// intentionally NOT defined — the stub does not model windowed-feed
/// violations and there is no bootloader to reboot into. `setWindow()` returns
/// false; `rebootIntoBootloader()` returns false.

#include "fl/wdt/watchdog.h"
#include "fl/stl/atomic.h"
#include "fl/stl/chrono.h"
#include "fl/stl/cstring.h"
#include "fl/stl/mutex.h"
#include "fl/stl/thread.h"

#define FL_WATCHDOG_HAS_HARDWARE
#define FL_WATCHDOG_PERSIST_BYTES 16
#define FL_WATCHDOG_MAX_TIMEOUT_MS 600000u
#define FL_WATCHDOG_HAS_PRE_TIMEOUT_IRQ
#define FL_WATCHDOG_HAS_CRASH_REPORT

namespace fl {
namespace platforms {

// Persistent storage layout — survives within a single process. Magic word at
// the front separates first-run garbage from genuine persisted state.
//
// `crash_count` is touched by both the watchdog timer thread (bumped in
// stubWatchdogReset()) and the test thread (read in consecutiveCrashCount() /
// isInSafeMode(), zeroed in markCleanShutdown()) — promote it to atomic so
// TSAN doesn't flag a data race and the safe-mode / crash-counter assertions
// don't flake. The remaining persist fields are only mutated from the test
// thread (or under s.mu via reset_if_unmagic on first construction), so they
// stay plain memory.
struct StubWatchdogPersist {
    static constexpr fl::u32 kMagic = 0xFA57DDADu;
    fl::u32 magic;
    fl::atomic<fl::u16> crash_count;
    fl::u8  safe_mode_threshold;
    fl::u8  user[FL_WATCHDOG_PERSIST_BYTES];

    void reset_if_unmagic() FL_NOEXCEPT {
        if (magic != kMagic) {
            magic = kMagic;
            crash_count.store(0);
            safe_mode_threshold = 2;
            fl::memset(user, 0, sizeof(user));
        }
    }
};

inline StubWatchdogPersist& stubWatchdogPersist() FL_NOEXCEPT {
    static StubWatchdogPersist s{};  // okay static in header — single-TU `.impl.hpp`
    s.reset_if_unmagic();
    return s;
}

// State shared between fl::Watchdog methods and the timer thread.
//
// `reset_was_watchdog`, `fired`, and `software_reboot` are written from the
// worker thread and read/cleared from API calls; they must be atomic to avoid
// data races (and the same races flaking the tests under TSAN-like builds).
//
// `software_reboot` is kept separate from `reset_was_watchdog` so an
// intentional `reboot()` is reported as `ResetCause::SOFTWARE` and is not
// counted as a watchdog crash.
struct StubWatchdogState {
    fl::mutex                              mu;
    fl::atomic<bool>                       enabled;
    fl::atomic<bool>                       stop;
    fl::atomic<fl::u32>                    timeout_ms;
    fl::chrono::steady_clock::time_point   deadline;
    WatchdogTimeoutCallback                cb;
    void*                                  cb_user;
    fl::function<void()>                   cb_fn;
    fl::atomic<bool>                       reset_was_watchdog;
    fl::atomic<bool>                       fired;
    fl::atomic<bool>                       software_reboot;
    fl::thread                             worker;

    StubWatchdogState() FL_NOEXCEPT
        : enabled(false), stop(false), timeout_ms(0),
          cb(nullptr), cb_user(nullptr),
          reset_was_watchdog(false), fired(false), software_reboot(false) {}

    ~StubWatchdogState() {
        stop.store(true);
        if (worker.joinable()) worker.join();
    }
};

inline StubWatchdogState& stubWatchdogState() FL_NOEXCEPT {
    static StubWatchdogState s{};  // okay static in header — single-TU `.impl.hpp`
    return s;
}

// Worker-thread side of a watchdog timeout: bump the crash counter and flag
// the reset as watchdog-induced. Does NOT mark `software_reboot`.
//
// `crash_count` is incremented via a saturating CAS loop so concurrent
// markCleanShutdown() resets are not lost and the counter never rolls over
// from 0xFFFF back to 0.
inline void stubWatchdogReset() FL_NOEXCEPT {
    auto& p = stubWatchdogPersist();
    p.magic = StubWatchdogPersist::kMagic;
    fl::u16 current = p.crash_count.load();
    while (current < 0xFFFF) {
        if (p.crash_count.compare_exchange_weak(current, static_cast<fl::u16>(current + 1))) {
            break;
        }
        // current was reloaded by compare_exchange_weak — retry.
    }

    auto& s = stubWatchdogState();
    s.reset_was_watchdog.store(true);
    s.fired.store(true);
}

// Explicit `Watchdog::reboot()` path: mark the reset as SOFTWARE without
// bumping crash_count or setting the watchdog-induced flag. Real-hardware
// platforms similarly distinguish ESP_RST_SW from ESP_RST_TASK_WDT.
inline void stubWatchdogSoftwareReboot() FL_NOEXCEPT {
    auto& p = stubWatchdogPersist();
    p.magic = StubWatchdogPersist::kMagic;

    auto& s = stubWatchdogState();
    s.software_reboot.store(true);
}

inline void stubWatchdogTimerLoop() FL_NOEXCEPT {
    auto& s = stubWatchdogState();
    while (!s.stop.load()) {
        // Host-only watchdog timer thread; not part of async-scheduler pumping.
        fl::this_thread::sleep_for(fl::chrono::milliseconds(10)); // ok sleep for
        if (!s.enabled.load()) continue;
        fl::chrono::steady_clock::time_point dl;
        {
            fl::lock_guard<fl::mutex> g(s.mu);
            dl = s.deadline;
        }
        if (fl::chrono::steady_clock::now() >= dl) {
            WatchdogTimeoutCallback cb;
            void* user;
            fl::function<void()> cb_fn_copy;
            {
                fl::lock_guard<fl::mutex> g(s.mu);
                cb = s.cb;
                user = s.cb_user;
                cb_fn_copy = s.cb_fn;
                s.enabled.store(false);
            }
            if (cb) cb(user);
            if (cb_fn_copy) cb_fn_copy();
            stubWatchdogReset();
            // Don't exit the loop — stay alive so subsequent begin() calls
            // re-arm correctly. `enabled` is already false.
        }
    }
}

inline void stubWatchdogEnsureWorker() FL_NOEXCEPT {
    auto& s = stubWatchdogState();
    fl::lock_guard<fl::mutex> g(s.mu);
    if (!s.worker.joinable()) {
        s.worker = fl::thread(stubWatchdogTimerLoop);
    }
}

} // namespace platforms

// ---- fl::Watchdog method definitions ----
// `Watchdog::instance()` is defined here (and in every sibling `.impl.hpp` /
// the noop) so the function-local static lives in exactly one TU per program.
// Avoids the Teensy 3.x `__cxa_guard` ABI conflict that a header-side
// definition would impose on every TU including FastLED.h.
Watchdog& Watchdog::instance() FL_NOEXCEPT {
    static Watchdog sInstance;
    return sInstance;
}

void Watchdog::begin(fl::u32 timeout_ms) FL_NOEXCEPT {
    if (timeout_ms == 0) timeout_ms = 1;
    if (timeout_ms > FL_WATCHDOG_MAX_TIMEOUT_MS) timeout_ms = FL_WATCHDOG_MAX_TIMEOUT_MS;
    platforms::stubWatchdogEnsureWorker();
    auto& s = platforms::stubWatchdogState();
    fl::lock_guard<fl::mutex> g(s.mu);
    s.timeout_ms.store(timeout_ms);
    s.deadline = fl::chrono::steady_clock::now() + fl::chrono::milliseconds(timeout_ms);
    s.enabled.store(true);
}

void Watchdog::feed() FL_NOEXCEPT {
    auto& s = platforms::stubWatchdogState();
    if (!s.enabled.load()) return;
    fl::lock_guard<fl::mutex> g(s.mu);
    s.deadline = fl::chrono::steady_clock::now() + fl::chrono::milliseconds(s.timeout_ms.load());
}

void Watchdog::disable() FL_NOEXCEPT {
    auto& s = platforms::stubWatchdogState();
    s.enabled.store(false);
}

ResetCause Watchdog::lastResetCause() const FL_NOEXCEPT {
    auto& s = platforms::stubWatchdogState();
    // Watchdog-induced reset takes precedence over a software reboot, but
    // never get conflated: a clean `reboot()` reports SOFTWARE, a timeout
    // reports WATCHDOG, and a fresh process reports POWER_ON.
    if (s.reset_was_watchdog.load()) return ResetCause::WATCHDOG;
    if (s.software_reboot.load())    return ResetCause::SOFTWARE;
    return ResetCause::POWER_ON;
}

bool Watchdog::lastResetWasWatchdog() const FL_NOEXCEPT {
    return platforms::stubWatchdogState().reset_was_watchdog.load();
}

fl::u8 Watchdog::persistRead(fl::size idx) const FL_NOEXCEPT {
    if (idx >= FL_WATCHDOG_PERSIST_BYTES) return 0;
    return platforms::stubWatchdogPersist().user[idx];
}

void Watchdog::persistWrite(fl::size idx, fl::u8 v) FL_NOEXCEPT {
    if (idx >= FL_WATCHDOG_PERSIST_BYTES) return;
    platforms::stubWatchdogPersist().user[idx] = v;
}

fl::u16 Watchdog::consecutiveCrashCount() const FL_NOEXCEPT {
    return platforms::stubWatchdogPersist().crash_count.load();
}

void Watchdog::markCleanShutdown() FL_NOEXCEPT {
    platforms::stubWatchdogPersist().crash_count.store(0);
}

bool Watchdog::isInSafeMode() const FL_NOEXCEPT {
    return platforms::stubWatchdogPersist().crash_count.load() >= mSafeModeThreshold;
}

fl::u16 Watchdog::safeModeThreshold() const FL_NOEXCEPT {
    return mSafeModeThreshold;
}

void Watchdog::setSafeModeThreshold(fl::u16 t) FL_NOEXCEPT {
    mSafeModeThreshold = t;
}

FL_NORETURN void Watchdog::reboot() FL_NOEXCEPT {
    // Intentional software reboot — does NOT bump crash_count and does NOT
    // mark the reset as watchdog-induced.
    platforms::stubWatchdogSoftwareReboot();
    while (true) {}
}

bool Watchdog::onTimeout(WatchdogTimeoutCallback cb, void* user_data) FL_NOEXCEPT {
    auto& s = platforms::stubWatchdogState();
    fl::lock_guard<fl::mutex> g(s.mu);
    s.cb = cb;
    s.cb_user = user_data;
    return true;
}

bool Watchdog::onTimeout(fl::function<void()> cb) FL_NOEXCEPT {
    auto& s = platforms::stubWatchdogState();
    fl::lock_guard<fl::mutex> g(s.mu);
    s.cb_fn = cb;
    return true;
}

bool Watchdog::setPauseOnDebug(bool /*pause*/) FL_NOEXCEPT { return true; }
bool Watchdog::writeCrashLog(fl::span<const fl::u8> /*payload*/) FL_NOEXCEPT { return false; }
fl::size Watchdog::readCrashLog(fl::span<fl::u8> /*out*/) const FL_NOEXCEPT { return 0; }
bool Watchdog::rebootIntoBootloader() FL_NOEXCEPT { return false; }

bool Watchdog::setWindow(fl::u32 /*min*/, fl::u32 /*max*/) FL_NOEXCEPT {
    // Stub does NOT model windowed feeds (no min/max enforcement in feed() or
    // the timer loop). Returning false matches FL_WATCHDOG_HAS_WINDOW_MODE
    // not being defined and keeps user code that checks the return value
    // honest.
    return false;
}
bool Watchdog::hasCrashReport() const FL_NOEXCEPT {
    return platforms::stubWatchdogState().reset_was_watchdog.load();
}
WatchdogCrashReport Watchdog::readCrashReport() const FL_NOEXCEPT {
    WatchdogCrashReport r{};
    r.valid = platforms::stubWatchdogState().reset_was_watchdog.load();
    r.fault_type = r.valid ? "WatchdogTimeout (stub)" : "";
    return r;
}
void Watchdog::clearCrashReport() FL_NOEXCEPT {
    // Clear ONLY the watchdog-crash state — `software_reboot` is independent
    // bookkeeping and must not be silently reset, otherwise lastResetCause()
    // would flip back to POWER_ON after a clean reboot+clear sequence.
    auto& s = platforms::stubWatchdogState();
    s.reset_was_watchdog.store(false);
    s.fired.store(false);
}

} // namespace fl
