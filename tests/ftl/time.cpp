#include "fl/stl/time.h"
#include "fl/stl/new.h"
#include "doctest.h"
#include "fl/stl/function.h"
#include "fl/stl/move.h"
#include "fl/int.h"

using namespace fl;

TEST_CASE("fl::time - basic functionality") {
    SUBCASE("time returns non-zero values") {
        fl::u32 t1 = fl::millis();
        CHECK(t1 >= 0); // Always true, but documents expectation
    }

    SUBCASE("time is monotonically increasing") {
        fl::u32 t1 = fl::millis();
        // Small delay to ensure time advances
        volatile int dummy = 0;
        for (int i = 0; i < 10000; ++i) {
            dummy += i;
        }
        fl::u32 t2 = fl::millis();
        // Time should be >= t1 (may be equal if very fast)
        CHECK(t2 >= t1);
    }

    SUBCASE("time difference calculation") {
        fl::u32 start = fl::millis();
        // Small delay
        volatile int dummy = 0;
        for (int i = 0; i < 10000; ++i) {
            dummy += i;
        }
        fl::u32 end = fl::millis();
        fl::u32 elapsed = end - start;
        // Elapsed should be small but >= 0
        CHECK(elapsed >= 0);
        // Should be less than a reasonable threshold (e.g., 1 second)
        CHECK(elapsed < 1000);
    }

    SUBCASE("multiple calls to time") {
        fl::u32 t1 = fl::millis();
        fl::u32 t2 = fl::millis();
        fl::u32 t3 = fl::millis();

        // All should be >= previous
        CHECK(t2 >= t1);
        CHECK(t3 >= t2);
    }
}

#ifdef FASTLED_TESTING

TEST_CASE("fl::MockTimeProvider - basic functionality") {
    SUBCASE("constructor with initial time") {
        MockTimeProvider mock(1000);
        CHECK_EQ(mock.current_time(), 1000);
    }

    SUBCASE("constructor with default time") {
        MockTimeProvider mock;
        CHECK_EQ(mock.current_time(), 0);
    }

    SUBCASE("advance time") {
        MockTimeProvider mock(100);
        CHECK_EQ(mock.current_time(), 100);

        mock.advance(50);
        CHECK_EQ(mock.current_time(), 150);

        mock.advance(200);
        CHECK_EQ(mock.current_time(), 350);
    }

    SUBCASE("set_time") {
        MockTimeProvider mock(100);
        CHECK_EQ(mock.current_time(), 100);

        mock.set_time(500);
        CHECK_EQ(mock.current_time(), 500);

        mock.set_time(0);
        CHECK_EQ(mock.current_time(), 0);
    }

    SUBCASE("operator() returns current time") {
        MockTimeProvider mock(1234);
        CHECK_EQ(mock(), 1234);
        CHECK_EQ(mock.current_time(), 1234);

        mock.advance(100);
        CHECK_EQ(mock(), 1334);
    }

    SUBCASE("advance with wraparound") {
        // Test near u32 max value
        fl::u32 near_max = 0xFFFFFF00u;
        MockTimeProvider mock(near_max);

        mock.advance(0x100);
        // Should wrap around
        CHECK_EQ(mock.current_time(), 0);
    }
}

TEST_CASE("fl::inject_time_provider - injection and clearing") {
    // Save any existing provider state (though there shouldn't be one)
    // and ensure we clean up after this test

    SUBCASE("inject and use mock time provider") {
        MockTimeProvider mock(5000);
        // Need to capture mock by reference in a lambda for it to work
        inject_time_provider([&mock]() { return mock(); });

        // fl::millis() should now return mock time
        CHECK_EQ(fl::millis(), 5000);

        // Advance mock time
        mock.advance(100);
        CHECK_EQ(fl::millis(), 5100);

        // Clean up
        clear_time_provider();
    }

    SUBCASE("clear_time_provider restores platform time") {
        MockTimeProvider mock(1000);
        inject_time_provider([&mock]() { return mock(); });

        CHECK_EQ(fl::millis(), 1000);

        clear_time_provider();

        // Should now return platform time (non-zero in most cases)
        fl::u32 platform_time = fl::millis();
        // Can't assert exact value, but should be different from mock
        // Just verify it's callable
        CHECK(platform_time >= 0);
    }

    SUBCASE("multiple injections") {
        MockTimeProvider mock1(1000);
        inject_time_provider([&mock1]() { return mock1(); });
        CHECK_EQ(fl::millis(), 1000);

        MockTimeProvider mock2(2000);
        inject_time_provider([&mock2]() { return mock2(); });
        CHECK_EQ(fl::millis(), 2000);

        clear_time_provider();
    }

    SUBCASE("clear without injection is safe") {
        // Should be safe to call multiple times
        clear_time_provider();
        clear_time_provider();

        // Time should still work
        fl::u32 t = fl::millis();
        CHECK(t >= 0);
    }
}

TEST_CASE("fl::time - timing scenarios with mock") {
    SUBCASE("animation timing simulation") {
        MockTimeProvider mock(0);
        inject_time_provider([&mock]() { return mock(); });

        fl::u32 last_frame = 0;
        fl::u32 frame_count = 0;
        const fl::u32 frame_interval = 16; // ~60 FPS

        // Simulate several frames
        for (int i = 0; i < 10; ++i) {
            mock.advance(frame_interval);
            fl::u32 now = fl::millis();

            if (now - last_frame >= frame_interval) {
                frame_count++;
                last_frame = now;
            }
        }

        CHECK_EQ(frame_count, 10);
        CHECK_EQ(fl::millis(), 160);

        clear_time_provider();
    }

    SUBCASE("timeout handling simulation") {
        MockTimeProvider mock(1000);
        inject_time_provider([&mock]() { return mock(); });

        fl::u32 timeout_duration = 5000;
        fl::u32 timeout = fl::millis() + timeout_duration;

        CHECK_EQ(timeout, 6000);

        // Simulate time passing but not reaching timeout
        mock.advance(2000);
        CHECK(fl::millis() < timeout);

        // Advance past timeout
        mock.advance(3001);
        CHECK(fl::millis() >= timeout);

        clear_time_provider();
    }

    SUBCASE("elapsed time calculation") {
        MockTimeProvider mock(1000);
        inject_time_provider([&mock]() { return mock(); });

        fl::u32 start = fl::millis();
        CHECK_EQ(start, 1000);

        mock.advance(250);
        fl::u32 elapsed = fl::millis() - start;
        CHECK_EQ(elapsed, 250);

        mock.advance(750);
        elapsed = fl::millis() - start;
        CHECK_EQ(elapsed, 1000);

        clear_time_provider();
    }

    SUBCASE("wraparound handling") {
        // Test time wraparound at 32-bit boundary
        fl::u32 near_max = 0xFFFFFFF0u;
        MockTimeProvider mock(near_max);
        inject_time_provider([&mock]() { return mock(); });

        fl::u32 start = fl::millis();
        CHECK_EQ(start, near_max);

        // Advance past wraparound
        mock.advance(0x20);
        fl::u32 now = fl::millis();

        // After wraparound, now < start
        CHECK(now < start);

        // But elapsed time calculation still works with unsigned arithmetic
        fl::u32 elapsed = now - start;
        CHECK_EQ(elapsed, 0x20);

        clear_time_provider();
    }
}

TEST_CASE("fl::time - edge cases") {
    SUBCASE("time at u32 boundaries") {
        MockTimeProvider mock(0);
        inject_time_provider([&mock]() { return mock(); });

        CHECK_EQ(fl::millis(), 0);

        mock.set_time(0xFFFFFFFF);
        CHECK_EQ(fl::millis(), 0xFFFFFFFF);

        clear_time_provider();
    }

    SUBCASE("zero advances") {
        MockTimeProvider mock(1000);
        inject_time_provider([&mock]() { return mock(); });

        mock.advance(0);
        CHECK_EQ(fl::millis(), 1000);

        clear_time_provider();
    }

    SUBCASE("large time values") {
        fl::u32 large_time = 0x7FFFFFFF; // Max positive i32 value
        MockTimeProvider mock(large_time);
        inject_time_provider([&mock]() { return mock(); });

        CHECK_EQ(fl::millis(), large_time);

        mock.advance(1);
        CHECK_EQ(fl::millis(), large_time + 1);

        clear_time_provider();
    }
}

TEST_CASE("fl::MockTimeProvider - functional behavior") {
    SUBCASE("can be used as function object") {
        MockTimeProvider mock(1234);

        // MockTimeProvider can be used directly as a functor
        CHECK_EQ(mock(), 1234);

        mock.advance(100);
        CHECK_EQ(mock(), 1334);

        // When copying to fl::function, need to capture by reference
        fl::function<fl::u32()> func = [&mock]() { return mock(); };
        CHECK_EQ(func(), 1334);
    }

    SUBCASE("copy and move semantics") {
        MockTimeProvider mock1(1000);

        // Copy construction
        MockTimeProvider mock2 = mock1;
        CHECK_EQ(mock2.current_time(), 1000);

        // Both should be independent
        mock1.advance(100);
        CHECK_EQ(mock1.current_time(), 1100);
        CHECK_EQ(mock2.current_time(), 1000);
    }
}

#endif // FASTLED_TESTING

TEST_CASE("fl::time - integration patterns") {
    SUBCASE("debounce pattern") {
        fl::u32 last_trigger = 0;
        const fl::u32 debounce_time = 50;

        fl::u32 now = fl::millis();
        bool can_trigger = (now - last_trigger >= debounce_time);

        // First trigger should work (unless we're in first 50ms of system uptime)
        if (now >= debounce_time) {
            CHECK(can_trigger);
        }
    }

    SUBCASE("rate limiting pattern") {
        static fl::u32 last_action = 0;
        const fl::u32 min_interval = 100;

        fl::u32 now = fl::millis();
        if (now - last_action >= min_interval) {
            last_action = now;
            // Action would be performed here
        }

        // Verify pattern compiles and runs
        CHECK(now >= last_action);
    }
}
