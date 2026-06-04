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

    dog.begin(50);

    // Wait long enough for the stub timer thread (10 ms tick) to detect
    // expiry + run the callback + bump the counter.
    fl::this_thread::sleep_for(fl::chrono::milliseconds(300));  // ok sleep for

    FL_CHECK(fired.load());
    FL_CHECK(dog.consecutiveCrashCount() >= 1);
    FL_CHECK(dog.lastResetWasWatchdog());

    dog.clearCrashReport();
    dog.markCleanShutdown();
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

    dog.begin(200);
    for (int i = 0; i < 10; ++i) {
        fl::this_thread::sleep_for(fl::chrono::milliseconds(50));  // ok sleep for
        dog.feed();
    }
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
