// ok cpp include
//
// Unit tests for fl::Watchdog (the unified cross-platform watchdog API).
// Runs against the stub host implementation (`platforms/stub/watchdog_stub.impl.hpp`).
//
// FASTLED_STUB_WATCHDOG_NO_ABORT is set so the timeout path doesn't kill the
// test runner — instead it bumps the crash counter and returns, which is
// exactly what we need to verify safe-mode behavior.

#define FASTLED_STUB_WATCHDOG_NO_ABORT 1

#include "fl/wdt/watchdog.h"
#include "platforms/stub/time_stub.h"
#include "fl/stl/atomic.h"
#include "fl/stl/chrono.h"
#include "fl/stl/thread.h"
#include "test.h"

using namespace fl;

FL_TEST_CASE("fl::Watchdog — singleton accessor returns same instance") {
    Watchdog& a = Watchdog::instance();
    Watchdog& b = Watchdog::instance();
    FL_CHECK_EQ(&a, &b);
}

FL_TEST_CASE("fl::Watchdog — Tier 0 begin/feed/disable doesn't crash") {
    Watchdog& dog = Watchdog::instance();
    dog.disable();
    dog.begin(10000);
    dog.feed();
    dog.disable();
}

// Mirrors `FL_WATCHDOG_PERSIST_BYTES` from `platforms/stub/watchdog_stub.impl.hpp`.
// The capability macro is only defined inside the impl TU (it's `// IWYU
// pragma: private`), so the test holds a single-source mirror here so the
// boundary asserts derive from the contract value rather than a magic
// literal. If the stub's persist size ever changes, this constant must be
// updated to match.
static constexpr fl::size kStubPersistBytes = 16;

FL_TEST_CASE("fl::Watchdog — persist read/write within bounds") {
    Watchdog& dog = Watchdog::instance();
    // Derive indices from the contract so this test guards the actual
    // boundary at `idx == kStubPersistBytes`, not a magic constant.
    constexpr fl::size kLastValid = kStubPersistBytes - 1;
    dog.persistWrite(0, 0xAB);
    dog.persistWrite(kLastValid, 0xCD);
    FL_CHECK_EQ(dog.persistRead(0), 0xAB);
    FL_CHECK_EQ(dog.persistRead(kLastValid), 0xCD);
}

FL_TEST_CASE("fl::Watchdog — persist read/write out of bounds is safe") {
    Watchdog& dog = Watchdog::instance();
    // First invalid index is exactly the boundary — exercises the off-by-one
    // edge that magic indices like `9999` would silently skip.
    constexpr fl::size kFirstInvalid = kStubPersistBytes;
    dog.persistWrite(kFirstInvalid, 0xFF);   // ignored
    FL_CHECK_EQ(dog.persistRead(kFirstInvalid), 0);
    dog.persistWrite(9999, 0xFF);            // far out-of-bounds also ignored
    FL_CHECK_EQ(dog.persistRead(9999), 0);
}

FL_TEST_CASE("fl::Watchdog — safe-mode threshold default is 2") {
    Watchdog& dog = Watchdog::instance();
    FL_CHECK_EQ(dog.safeModeThreshold(), 2);
}

FL_TEST_CASE("fl::Watchdog — setSafeModeThreshold updates the threshold") {
    Watchdog& dog = Watchdog::instance();
    dog.setSafeModeThreshold(5);
    FL_CHECK_EQ(dog.safeModeThreshold(), 5);
    dog.setSafeModeThreshold(2);   // restore
}

FL_TEST_CASE("fl::Watchdog — markCleanShutdown resets crash counter") {
    Watchdog& dog = Watchdog::instance();
    dog.markCleanShutdown();
    FL_CHECK_EQ(dog.consecutiveCrashCount(), 0);
    FL_CHECK_FALSE(dog.isInSafeMode());
}

FL_TEST_CASE("fl::Watchdog — Tier 1 onTimeout C-callback registers") {
    Watchdog& dog = Watchdog::instance();
    static int fired = 0;
    fired = 0;
    bool ok = dog.onTimeout([](void* ud) {
        *static_cast<int*>(ud) = 42;
    }, &fired);
    FL_CHECK(ok);
}

FL_TEST_CASE("fl::Watchdog — Tier 1 onTimeout fl::function overload registers") {
    Watchdog& dog = Watchdog::instance();
    bool ok = dog.onTimeout(fl::function<void()>([]() { /* nothing */ }));
    FL_CHECK(ok);
}

FL_TEST_CASE("fl::Watchdog — Tier 1 surface, flash and bootloader return false on stub") {
    Watchdog& dog = Watchdog::instance();
    // setPauseOnDebug returns true on stub
    FL_CHECK(dog.setPauseOnDebug(true));
    // writeCrashLog / readCrashLog not backed by stub flash → false / 0
    fl::u8 buf[4] = {0};
    FL_CHECK_FALSE(dog.writeCrashLog(fl::span<const fl::u8>(buf, 4)));
    FL_CHECK_EQ(dog.readCrashLog(fl::span<fl::u8>(buf, 4)), fl::size{0});
    // No bootloader-reboot path on host
    FL_CHECK_FALSE(dog.rebootIntoBootloader());
}

FL_TEST_CASE("fl::Watchdog — Tier 2 setWindow returns false on stub (no window-mode support)") {
    // Stub does not implement windowed-feed enforcement — `setWindow()` must
    // return false to match `FL_WATCHDOG_HAS_WINDOW_MODE` NOT being defined,
    // so user code that branches on the return value behaves correctly.
    Watchdog& dog = Watchdog::instance();
    FL_CHECK_FALSE(dog.setWindow(10, 1000));
}

FL_TEST_CASE("fl::Watchdog — Tier 2 crash report API surface") {
    Watchdog& dog = Watchdog::instance();
    dog.clearCrashReport();
    FL_CHECK_FALSE(dog.hasCrashReport());
    WatchdogCrashReport r = dog.readCrashReport();
    FL_CHECK_FALSE(r.valid);
}

// ---------------------------------------------------------------------------
// Deterministic clock for the two timeout tests.
//
// These previously drove real time with sleep_for. That only guarantees a
// LOWER bound, so on a loaded CI runner a single 50 ms sleep could overshoot
// the 200 ms timeout, the correctly-fed watchdog would expire exactly as
// designed, and the "does not fire" assertion would fail for a reason that is
// not a bug (observed on macOS, FastLED#3978).
//
// Freezing the stub's clock removes wall time from the equation: the watchdog
// now fires precisely when the test advances past the deadline.
//
// The stub derives millis(), micros(), and fl::chrono::steady_clock (which is
// what the watchdog's deadline is built on) from the single root
// fl::platforms::micros64(), so overriding that one function is enough. Note
// fl::inject_time_provider() does NOT work here — it covers fl::millis() only.
namespace {

// Read from the watchdog's timer thread as well as the test thread, so atomic.
fl::atomic<fl::u64> gMockNowUs{0};

fl::u64 mockMicros64() {
    return gMockNowUs.load();
}

// RAII so an early FL_CHECK failure cannot leave the clock frozen and corrupt
// every later test case in this binary.
struct ScopedMockClock {
    explicit ScopedMockClock(fl::u64 start_ms = 100000) {
        gMockNowUs.store(start_ms * 1000u);
        setClockFunction(&mockMicros64);
    }
    ~ScopedMockClock() { clearClockFunction(); }

    void advance(fl::u32 ms) {
        gMockNowUs.store(gMockNowUs.load() + static_cast<fl::u64>(ms) * 1000u);
    }
};

// The worker thread observes the clock on a real 10 ms tick, so a test that
// expects a timeout must still wait for that observation after advancing. This
// direction is safe: waiting longer can never cause a false failure, only a
// slower pass. Bounded so a genuine regression fails instead of hanging.
bool waitForFlag(fl::atomic<bool>& flag, fl::u32 timeout_ms = 2000) {
    for (fl::u32 waited = 0; waited < timeout_ms; waited += 5) {
        if (flag.load()) return true;
        fl::this_thread::sleep_for(fl::chrono::milliseconds(5));  // ok sleep for
    }
    return flag.load();
}

}  // namespace

FL_TEST_CASE("fl::Watchdog — timeout fires callback and increments crash counter") {
    Watchdog& dog = Watchdog::instance();
    dog.markCleanShutdown();
    dog.clearCrashReport();

    // `fired` is written from the stub's worker thread and read here on the
    // test thread; `volatile` does NOT provide cross-thread atomicity, so use
    // fl::atomic<bool>.
    static fl::atomic<bool> fired;
    fired.store(false);
    dog.onTimeout([](void*) { fired.store(true); }, nullptr);

    ScopedMockClock clock;
    dog.begin(50);

    // Push mock time past the deadline, then wait for the worker's 10 ms tick
    // to observe it. Unlike the old fixed 300 ms sleep, this cannot expire
    // early: no real duration can advance the mocked clock.
    clock.advance(100);

    FL_CHECK(waitForFlag(fired));
    FL_CHECK(dog.consecutiveCrashCount() >= 1);
    FL_CHECK(dog.lastResetWasWatchdog());

    dog.clearCrashReport();
    dog.markCleanShutdown();
}

// Wrap robustness. fl::millis() is u32 and rolls over roughly every 49.7 days;
// micros() rolls over every ~71.6 minutes. An implementation that stored an
// absolute deadline would compute an unreachable one just below the rollover
// and then silently never fire. Expiry is therefore an unsigned elapsed
// comparison (`now - fed_at >= timeout`), which wraps in the same direction as
// the clock and stays correct.
//
// Only testable at all because the clock is mockable — parking real time three
// seconds below 2^32 ms is not an option.
FL_TEST_CASE("fl::Watchdog — fires across a u32 millisecond wrap") {
    Watchdog& dog = Watchdog::instance();
    dog.markCleanShutdown();
    dog.clearCrashReport();

    static fl::atomic<bool> fired;
    fired.store(false);
    dog.onTimeout([](void*) { fired.store(true); }, nullptr);

    // Arm 3 s before the u32 millisecond rollover, then step over it.
    constexpr fl::u64 kJustBelowWrapMs = 0xFFFFFFFFull - 3000ull;
    ScopedMockClock clock(kJustBelowWrapMs);
    dog.begin(1000);

    clock.advance(5000);  // crosses the boundary; elapsed is still 5000 ms

    FL_CHECK(waitForFlag(fired));

    dog.disable();
    dog.clearCrashReport();
    dog.markCleanShutdown();
}

// The other half: a fed watchdog must NOT fire across the wrap either. Guards
// against "fix" the wrap by making elapsed always look large.
FL_TEST_CASE("fl::Watchdog — feeding across a u32 wrap still prevents fire") {
    Watchdog& dog = Watchdog::instance();
    dog.markCleanShutdown();
    dog.clearCrashReport();

    static fl::atomic<bool> fired;
    fired.store(false);
    dog.onTimeout([](void*) { fired.store(true); }, nullptr);

    constexpr fl::u64 kJustBelowWrapMs = 0xFFFFFFFFull - 3000ull;
    ScopedMockClock clock(kJustBelowWrapMs);
    dog.begin(1000);

    for (int i = 0; i < 12; ++i) {
        clock.advance(500);  // half the timeout per feed, straddling the wrap
        dog.feed();
    }

    fl::this_thread::sleep_for(fl::chrono::milliseconds(50));  // ok sleep for
    dog.disable();

    FL_CHECK_FALSE(fired.load());
}

FL_TEST_CASE("fl::Watchdog — feeding before timeout prevents fire") {
    Watchdog& dog = Watchdog::instance();
    dog.markCleanShutdown();
    dog.clearCrashReport();
    fl::u16 baseline = dog.consecutiveCrashCount();

    // Same rationale as the previous test case — cross-thread flag must be
    // atomic, not volatile.
    static fl::atomic<bool> fired;
    fired.store(false);
    dog.onTimeout([](void*) { fired.store(true); }, nullptr);

    ScopedMockClock clock;
    dog.begin(200);
    for (int i = 0; i < 10; ++i) {
        // 50 ms of mock time per feed against a 200 ms timeout. Because the
        // clock only moves here, the deadline is provably never reached — no
        // amount of host scheduling jitter can change that.
        clock.advance(50);
        dog.feed();
    }

    // Give the worker several real ticks to prove it does NOT fire. Asserting
    // immediately would pass even if the deadline logic were broken, simply
    // because the worker had not looked yet.
    fl::this_thread::sleep_for(fl::chrono::milliseconds(50));  // ok sleep for
    dog.disable();

    FL_CHECK_FALSE(fired.load());
    FL_CHECK_EQ(dog.consecutiveCrashCount(), baseline);
}

FL_TEST_CASE("fl::Watchdog — isInSafeMode trips at threshold") {
    Watchdog& dog = Watchdog::instance();
    dog.markCleanShutdown();
    dog.setSafeModeThreshold(2);

    // Synthesize crashes via direct timeouts.
    for (int i = 0; i < 2; ++i) {
        dog.onTimeout([](void*) {}, nullptr);
        dog.begin(30);
        fl::this_thread::sleep_for(fl::chrono::milliseconds(200));  // ok sleep for
    }

    FL_CHECK(dog.consecutiveCrashCount() >= 2);
    FL_CHECK(dog.isInSafeMode());

    dog.markCleanShutdown();
    FL_CHECK_FALSE(dog.isInSafeMode());
}

// =============================================================================
// New API tests (issue #2755): resetCauseName + ResetInfo + ScopedWatchdog
// =============================================================================

FL_TEST_CASE("fl::resetCauseName — every enum value maps to a non-empty static view") {
    FL_CHECK_EQ(fl::resetCauseName(ResetCause::UNKNOWN),      fl::string_view("UNKNOWN"));
    FL_CHECK_EQ(fl::resetCauseName(ResetCause::POWER_ON),     fl::string_view("POWER_ON"));
    FL_CHECK_EQ(fl::resetCauseName(ResetCause::BROWNOUT),     fl::string_view("BROWNOUT"));
    FL_CHECK_EQ(fl::resetCauseName(ResetCause::EXTERNAL_PIN), fl::string_view("EXTERNAL_PIN"));
    FL_CHECK_EQ(fl::resetCauseName(ResetCause::WATCHDOG),     fl::string_view("WATCHDOG"));
    FL_CHECK_EQ(fl::resetCauseName(ResetCause::SOFTWARE),     fl::string_view("SOFTWARE"));
    FL_CHECK_EQ(fl::resetCauseName(ResetCause::LOCKUP),       fl::string_view("LOCKUP"));
    FL_CHECK_EQ(fl::resetCauseName(ResetCause::DEBUGGER),     fl::string_view("DEBUGGER"));
    FL_CHECK_EQ(fl::resetCauseName(ResetCause::PANIC),        fl::string_view("PANIC"));
}

FL_TEST_CASE("fl::ResetInfo::describe — terse format without subcause or verbose flag") {
    ResetInfo info{};
    info.cause = ResetCause::WATCHDOG;
    info.subcauseId = 0;
    info.rawRegister = 0xDEADBEEFu;

    char buf[64];
    fl::size n = info.describe(fl::span<char>(buf, sizeof(buf)), /*verbose=*/false);
    FL_CHECK_EQ(n, fl::size{8});  // "WATCHDOG"
    FL_CHECK_EQ(fl::string_view(buf, n), fl::string_view("WATCHDOG"));
}

FL_TEST_CASE("fl::ResetInfo::describe — verbose appends raw=0x...") {
    ResetInfo info{};
    info.cause = ResetCause::WATCHDOG;
    info.subcauseId = 0;
    info.rawRegister = 0x000000FFu;

    char buf[64];
    fl::size n = info.describe(fl::span<char>(buf, sizeof(buf)), /*verbose=*/true);
    // "WATCHDOG raw=0x000000ff" = 8 + 5 + 10 = 23 chars
    FL_CHECK_EQ(fl::string_view(buf, n), fl::string_view("WATCHDOG raw=0x000000ff"));
}

FL_TEST_CASE("fl::ResetInfo::describe — writes NUL when buffer has room") {
    ResetInfo info{};
    info.cause = ResetCause::POWER_ON;
    char buf[32] = { 'X', 'X', 'X', 'X', 'X', 'X', 'X', 'X', 0, };
    fl::size n = info.describe(fl::span<char>(buf, sizeof(buf)), false);
    FL_CHECK_EQ(buf[n], '\0');
}

FL_TEST_CASE("fl::ResetInfo::describe — truncates safely when buffer is too small") {
    ResetInfo info{};
    info.cause = ResetCause::EXTERNAL_PIN;
    char buf[5];  // "EXTERNAL_PIN" needs 12, way more than 5
    fl::size n = info.describe(fl::span<char>(buf, sizeof(buf)), false);
    FL_CHECK_EQ(n, fl::size{5});  // Filled the buffer
    FL_CHECK_EQ(fl::string_view(buf, n), fl::string_view("EXTER"));
}

FL_TEST_CASE("fl::ResetInfo::describe — zero-sized buffer returns 0, no UB") {
    ResetInfo info{};
    info.cause = ResetCause::WATCHDOG;
    fl::size n = info.describe(fl::span<char>(nullptr, 0), true);
    FL_CHECK_EQ(n, fl::size{0});
}

FL_TEST_CASE("fl::Watchdog::lastResetInfo — default returns {cause, 0, 0}") {
    Watchdog& dog = Watchdog::instance();
    dog.clearCrashReport();
    dog.markCleanShutdown();
    ResetInfo info = dog.lastResetInfo();
    // On the stub, lastResetCause() returns POWER_ON by default.
    FL_CHECK(info.cause == ResetCause::POWER_ON || info.cause == ResetCause::WATCHDOG);
    FL_CHECK_EQ(info.subcauseId, fl::u8{0});
    FL_CHECK_EQ(info.rawRegister, fl::u32{0});
}

FL_TEST_CASE("fl::ScopedWatchdog — feeds on construction and destruction") {
    Watchdog& dog = Watchdog::instance();
    dog.markCleanShutdown();
    dog.clearCrashReport();
    dog.begin(500);
    // A bare scope here; constructor + destructor both feed. We can't
    // observe the feed directly through the API, but if the WDT was not
    // re-fed by the destructor the next loop iteration would race. Smoke
    // test: construct + destroy a guard, then sleep for under the timeout
    // and verify the dog did not fire.
    {
        ScopedWatchdog guard;  // default 15000ms — overrides our 500ms begin
        fl::this_thread::sleep_for(fl::chrono::milliseconds(50));  // ok sleep for
    }
    // The guard re-armed at 15000ms in firstInit, so the dog is fine here.
    FL_CHECK_FALSE(dog.lastResetWasWatchdog());
}

FL_TEST_CASE("fl::ScopedWatchdog — explicit timeout ctor compiles and feeds") {
    Watchdog& dog = Watchdog::instance();
    dog.markCleanShutdown();
    {
        ScopedWatchdog guard(20000u);  // explicit 20s
    }
    FL_CHECK_FALSE(dog.lastResetWasWatchdog());
}

FL_TEST_CASE("FL_WATCHDOG_AUTO macro — no-arg and arg forms both compile") {
    // Compile-time check that the macro expands correctly. The variable
    // name is stamped from __LINE__, so two on separate lines don't collide.
    FL_WATCHDOG_AUTO();
    FL_WATCHDOG_AUTO(5000);
    FL_WATCHDOG_AUTO(2000u);
    FL_CHECK(true);  // Always passes if it compiled
}

FL_TEST_CASE("fl::ScopedWatchdog — nesting detection counter") {
    // Validates the single-instance enforcement: the active-scope counter
    // increments on construction and decrements on destruction so that a
    // nested guard can be detected via the warning path.
    int baseline = ScopedWatchdog::activeScopeCount();
    {
        ScopedWatchdog outer;
        FL_CHECK_EQ(ScopedWatchdog::activeScopeCount(), baseline + 1);
        {
            ScopedWatchdog inner;  // triggers the WARN one-shot
            FL_CHECK_EQ(ScopedWatchdog::activeScopeCount(), baseline + 2);
        }
        FL_CHECK_EQ(ScopedWatchdog::activeScopeCount(), baseline + 1);
    }
    FL_CHECK_EQ(ScopedWatchdog::activeScopeCount(), baseline);
}
