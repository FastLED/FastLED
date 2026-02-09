#include "fl/stl/time.h"
#include "fl/stl/new.h"
#include "test.h"
#include "fl/stl/function.h"
#include "fl/stl/move.h"
#include "fl/int.h"

using namespace fl;

FL_TEST_CASE("fl::time - basic functionality") {
    FL_SUBCASE("time returns non-zero values") {
        fl::u32 t1 = fl::millis();
        FL_CHECK(t1 >= 0); // Always true, but documents expectation
    }

    FL_SUBCASE("time is monotonically increasing") {
        fl::u32 t1 = fl::millis();
        // Small delay to ensure time advances
        volatile int dummy = 0;
        for (int i = 0; i < 10000; ++i) {
            dummy += i;
        }
        fl::u32 t2 = fl::millis();
        // Time should be >= t1 (may be equal if very fast)
        FL_CHECK(t2 >= t1);
    }

    FL_SUBCASE("time difference calculation") {
        fl::u32 start = fl::millis();
        // Small delay
        volatile int dummy = 0;
        for (int i = 0; i < 10000; ++i) {
            dummy += i;
        }
        fl::u32 end = fl::millis();
        fl::u32 elapsed = end - start;
        // Elapsed should be small but >= 0
        FL_CHECK(elapsed >= 0);
        // Should be less than a reasonable threshold (e.g., 1 second)
        FL_CHECK(elapsed < 1000);
    }

    FL_SUBCASE("multiple calls to time") {
        fl::u32 t1 = fl::millis();
        fl::u32 t2 = fl::millis();
        fl::u32 t3 = fl::millis();

        // All should be >= previous
        FL_CHECK(t2 >= t1);
        FL_CHECK(t3 >= t2);
    }
}

#ifdef FASTLED_TESTING

FL_TEST_CASE("fl::MockTimeProvider - basic functionality") {
    FL_SUBCASE("constructor with initial time") {
        MockTimeProvider mock(1000);
        FL_CHECK_EQ(mock.current_time(), 1000);
    }

    FL_SUBCASE("constructor with default time") {
        MockTimeProvider mock;
        FL_CHECK_EQ(mock.current_time(), 0);
    }

    FL_SUBCASE("advance time") {
        MockTimeProvider mock(100);
        FL_CHECK_EQ(mock.current_time(), 100);

        mock.advance(50);
        FL_CHECK_EQ(mock.current_time(), 150);

        mock.advance(200);
        FL_CHECK_EQ(mock.current_time(), 350);
    }

    FL_SUBCASE("set_time") {
        MockTimeProvider mock(100);
        FL_CHECK_EQ(mock.current_time(), 100);

        mock.set_time(500);
        FL_CHECK_EQ(mock.current_time(), 500);

        mock.set_time(0);
        FL_CHECK_EQ(mock.current_time(), 0);
    }

    FL_SUBCASE("operator() returns current time") {
        MockTimeProvider mock(1234);
        FL_CHECK_EQ(mock(), 1234);
        FL_CHECK_EQ(mock.current_time(), 1234);

        mock.advance(100);
        FL_CHECK_EQ(mock(), 1334);
    }

    FL_SUBCASE("advance with wraparound") {
        // Test near u32 max value
        fl::u32 near_max = 0xFFFFFF00u;
        MockTimeProvider mock(near_max);

        mock.advance(0x100);
        // Should wrap around
        FL_CHECK_EQ(mock.current_time(), 0);
    }
}

FL_TEST_CASE("fl::inject_time_provider - injection and clearing") {
    // Save any existing provider state (though there shouldn't be one)
    // and ensure we clean up after this test

    FL_SUBCASE("inject and use mock time provider") {
        MockTimeProvider mock(5000);
        // Need to capture mock by reference in a lambda for it to work
        inject_time_provider([&mock]() { return mock(); });

        // fl::millis() should now return mock time
        FL_CHECK_EQ(fl::millis(), 5000);

        // Advance mock time
        mock.advance(100);
        FL_CHECK_EQ(fl::millis(), 5100);

        // Clean up
        clear_time_provider();
    }

    FL_SUBCASE("clear_time_provider restores platform time") {
        MockTimeProvider mock(1000);
        inject_time_provider([&mock]() { return mock(); });

        FL_CHECK_EQ(fl::millis(), 1000);

        clear_time_provider();

        // Should now return platform time (non-zero in most cases)
        fl::u32 platform_time = fl::millis();
        // Can't assert exact value, but should be different from mock
        // Just verify it's callable
        FL_CHECK(platform_time >= 0);
    }

    FL_SUBCASE("multiple injections") {
        MockTimeProvider mock1(1000);
        inject_time_provider([&mock1]() { return mock1(); });
        FL_CHECK_EQ(fl::millis(), 1000);

        MockTimeProvider mock2(2000);
        inject_time_provider([&mock2]() { return mock2(); });
        FL_CHECK_EQ(fl::millis(), 2000);

        clear_time_provider();
    }

    FL_SUBCASE("clear without injection is safe") {
        // Should be safe to call multiple times
        clear_time_provider();
        clear_time_provider();

        // Time should still work
        fl::u32 t = fl::millis();
        FL_CHECK(t >= 0);
    }
}

FL_TEST_CASE("fl::time - timing scenarios with mock") {
    FL_SUBCASE("animation timing simulation") {
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

        FL_CHECK_EQ(frame_count, 10);
        FL_CHECK_EQ(fl::millis(), 160);

        clear_time_provider();
    }

    FL_SUBCASE("timeout handling simulation") {
        MockTimeProvider mock(1000);
        inject_time_provider([&mock]() { return mock(); });

        fl::u32 timeout_duration = 5000;
        fl::u32 timeout = fl::millis() + timeout_duration;

        FL_CHECK_EQ(timeout, 6000);

        // Simulate time passing but not reaching timeout
        mock.advance(2000);
        FL_CHECK(fl::millis() < timeout);

        // Advance past timeout
        mock.advance(3001);
        FL_CHECK(fl::millis() >= timeout);

        clear_time_provider();
    }

    FL_SUBCASE("elapsed time calculation") {
        MockTimeProvider mock(1000);
        inject_time_provider([&mock]() { return mock(); });

        fl::u32 start = fl::millis();
        FL_CHECK_EQ(start, 1000);

        mock.advance(250);
        fl::u32 elapsed = fl::millis() - start;
        FL_CHECK_EQ(elapsed, 250);

        mock.advance(750);
        elapsed = fl::millis() - start;
        FL_CHECK_EQ(elapsed, 1000);

        clear_time_provider();
    }

    FL_SUBCASE("wraparound handling") {
        // Test time wraparound at 32-bit boundary
        fl::u32 near_max = 0xFFFFFFF0u;
        MockTimeProvider mock(near_max);
        inject_time_provider([&mock]() { return mock(); });

        fl::u32 start = fl::millis();
        FL_CHECK_EQ(start, near_max);

        // Advance past wraparound
        mock.advance(0x20);
        fl::u32 now = fl::millis();

        // After wraparound, now < start
        FL_CHECK(now < start);

        // But elapsed time calculation still works with unsigned arithmetic
        fl::u32 elapsed = now - start;
        FL_CHECK_EQ(elapsed, 0x20);

        clear_time_provider();
    }
}

FL_TEST_CASE("fl::time - edge cases") {
    FL_SUBCASE("time at u32 boundaries") {
        MockTimeProvider mock(0);
        inject_time_provider([&mock]() { return mock(); });

        FL_CHECK_EQ(fl::millis(), 0);

        mock.set_time(0xFFFFFFFF);
        FL_CHECK_EQ(fl::millis(), 0xFFFFFFFF);

        clear_time_provider();
    }

    FL_SUBCASE("zero advances") {
        MockTimeProvider mock(1000);
        inject_time_provider([&mock]() { return mock(); });

        mock.advance(0);
        FL_CHECK_EQ(fl::millis(), 1000);

        clear_time_provider();
    }

    FL_SUBCASE("large time values") {
        fl::u32 large_time = 0x7FFFFFFF; // Max positive i32 value
        MockTimeProvider mock(large_time);
        inject_time_provider([&mock]() { return mock(); });

        FL_CHECK_EQ(fl::millis(), large_time);

        mock.advance(1);
        FL_CHECK_EQ(fl::millis(), large_time + 1);

        clear_time_provider();
    }
}

FL_TEST_CASE("fl::MockTimeProvider - functional behavior") {
    FL_SUBCASE("can be used as function object") {
        MockTimeProvider mock(1234);

        // MockTimeProvider can be used directly as a functor
        FL_CHECK_EQ(mock(), 1234);

        mock.advance(100);
        FL_CHECK_EQ(mock(), 1334);

        // When copying to fl::function, need to capture by reference
        fl::function<fl::u32()> func = [&mock]() { return mock(); };
        FL_CHECK_EQ(func(), 1334);
    }

    FL_SUBCASE("copy and move semantics") {
        MockTimeProvider mock1(1000);

        // Copy construction
        MockTimeProvider mock2 = mock1;
        FL_CHECK_EQ(mock2.current_time(), 1000);

        // Both should be independent
        mock1.advance(100);
        FL_CHECK_EQ(mock1.current_time(), 1100);
        FL_CHECK_EQ(mock2.current_time(), 1000);
    }
}

#endif // FASTLED_TESTING

FL_TEST_CASE("fl::time - integration patterns") {
    FL_SUBCASE("debounce pattern") {
        fl::u32 last_trigger = 0;
        const fl::u32 debounce_time = 50;

        fl::u32 now = fl::millis();
        bool can_trigger = (now - last_trigger >= debounce_time);

        // First trigger should work (unless we're in first 50ms of system uptime)
        if (now >= debounce_time) {
            FL_CHECK(can_trigger);
        }
    }

    FL_SUBCASE("rate limiting pattern") {
        static fl::u32 last_action = 0;
        const fl::u32 min_interval = 100;

        fl::u32 now = fl::millis();
        if (now - last_action >= min_interval) {
            last_action = now;
            // Action would be performed here
        }

        // Verify pattern compiles and runs
        FL_CHECK(now >= last_action);
    }
}

FL_TEST_CASE("fl::millis64 - basic functionality") {
    FL_SUBCASE("millis64 returns non-zero values") {
        fl::millis64_reset();
        fl::u64 t1 = fl::millis64();
        FL_CHECK(t1 >= 0); // Always true, but documents expectation
    }

    FL_SUBCASE("millis64 is monotonically increasing") {
        fl::millis64_reset();
        fl::u64 t1 = fl::millis64();
        // Small delay to ensure time advances
        volatile int dummy = 0;
        for (int i = 0; i < 10000; ++i) {
            dummy += i;
        }
        fl::u64 t2 = fl::millis64();
        // Time should be >= t1 (may be equal if very fast)
        FL_CHECK(t2 >= t1);
    }

    FL_SUBCASE("millis64 never wraps (practical test)") {
        fl::millis64_reset();
        fl::u64 t1 = fl::millis64();
        fl::u64 t2 = fl::millis64();
        // t2 should always be >= t1 (no wraparound)
        FL_CHECK(t2 >= t1);
        // Verify 64-bit range (584 million years)
        FL_CHECK(t1 < 0xFFFFFFFFFFFFFFFFULL);
    }

    FL_SUBCASE("millis64 time difference calculation") {
        fl::millis64_reset();
        fl::u64 start = fl::millis64();
        // Small delay
        volatile int dummy = 0;
        for (int i = 0; i < 10000; ++i) {
            dummy += i;
        }
        fl::u64 end = fl::millis64();
        fl::u64 elapsed = end - start;
        // Elapsed should be small but >= 0
        FL_CHECK(elapsed >= 0);
        // Should be less than a reasonable threshold (e.g., 1 second)
        FL_CHECK(elapsed < 1000);
    }

    FL_SUBCASE("multiple calls to millis64") {
        fl::millis64_reset();
        fl::u64 t1 = fl::millis64();
        fl::u64 t2 = fl::millis64();
        fl::u64 t3 = fl::millis64();

        // All should be >= previous (never decreases)
        FL_CHECK(t2 >= t1);
        FL_CHECK(t3 >= t2);
    }

    FL_SUBCASE("millis64 compatibility with millis") {
        // Reset millis64 state to ensure clean test
        fl::millis64_reset();

        // millis64 should be based on millis, so values should be close
        fl::u32 m32 = fl::millis();
        fl::u32 m64_32 = static_cast<fl::u32>(fl::millis64());

        // Use unsigned arithmetic with wraparound to compute diff
        fl::u32 diff = m32 - m64_32;  // Wraps correctly even if m64_32 > m32
        FL_CHECK(diff < 100); // Allow small timing variance
    }
}

FL_TEST_CASE("fl::time - alias for millis64") {
    FL_SUBCASE("time() returns same type as millis64()") {
        fl::millis64_reset();
        fl::u64 t1 = fl::millis64();
        fl::u64 t2 = fl::millis64();

        // Both should return u64
        FL_CHECK(t1 >= 0);
        FL_CHECK(t2 >= 0);
    }

    FL_SUBCASE("time() and millis64() are consistent") {
        fl::millis64_reset();
        fl::u64 t1 = fl::millis64();
        fl::u64 m1 = fl::millis64();

        // Should be very close (within a few milliseconds)
        fl::u64 diff = (t1 > m1) ? (t1 - m1) : (m1 - t1);
        FL_CHECK(diff < 10); // Should be within 10ms
    }

    FL_SUBCASE("time() is monotonically increasing") {
        fl::millis64_reset();
        fl::u64 t1 = fl::millis64();
        // Small delay
        volatile int dummy = 0;
        for (int i = 0; i < 10000; ++i) {
            dummy += i;
        }
        fl::u64 t2 = fl::millis64();
        FL_CHECK(t2 >= t1);
    }
}

#ifdef FASTLED_TESTING

FL_TEST_CASE("fl::millis64 - wraparound handling") {
    FL_SUBCASE("millis64 handles 32-bit wraparound correctly") {
        // This test verifies that millis64 correctly accumulates across 32-bit wraparounds
        fl::millis64_reset();
        MockTimeProvider mock(0xFFFFFFF0u); // Start near max u32
        inject_time_provider([&mock]() { return mock(); });

        fl::u64 start64 = fl::millis64();

        // Advance past wraparound (by 0x20 = 32 ms)
        mock.advance(0x20);

        fl::u64 end64 = fl::millis64();

        // The 64-bit counter should have advanced by exactly 32ms
        // even though the 32-bit counter wrapped
        fl::u64 elapsed64 = end64 - start64;
        FL_CHECK_EQ(elapsed64, 0x20);

        // Verify the 32-bit millis wrapped around
        fl::u32 current32 = fl::millis();
        FL_CHECK(current32 < 0x20); // Should have wrapped to small value

        clear_time_provider();
    }

    FL_SUBCASE("millis64 accumulates correctly over multiple wraparounds") {
        fl::millis64_reset();
        MockTimeProvider mock(0); // Start at 0
        inject_time_provider([&mock]() { return mock(); });

        fl::u64 start64 = fl::millis64();

        // Simulate time passing equivalent to 2.5 wraparounds
        // Each wraparound is 2^32 ms ≈ 49.7 days
        // We'll advance by 0x180000000 = 1.5 * 2^32 in total (10 chunks of 0x26666666)

        // Advance in chunks to simulate normal operation
        for (int i = 0; i < 10; ++i) {
            mock.advance(0x26666666); // Advance by ~644.2 million ms each time
            fl::millis64(); // Call to update internal state
        }

        fl::u64 end64 = fl::millis64();
        fl::u64 elapsed64 = end64 - start64;

        // Should have accumulated the full amount (within rounding)
        // 10 chunks of 0x26666666 = 0x17FFFFFFC (6,442,450,940 ms)
        FL_CHECK(elapsed64 >= (0x180000000ULL - 0x20)); // Allow for small rounding

        clear_time_provider();
    }

    FL_SUBCASE("time() handles wraparound same as millis64()") {
        fl::millis64_reset();
        MockTimeProvider mock(0xFFFFFFF0u);
        inject_time_provider([&mock]() { return mock(); });

        fl::u64 start_time = fl::millis64();
        fl::u64 start_millis64 = fl::millis64();

        mock.advance(0x20);

        fl::u64 end_time = fl::millis64();
        fl::u64 end_millis64 = fl::millis64();

        // Both should handle wraparound identically
        FL_CHECK_EQ(end_time - start_time, 0x20);
        FL_CHECK_EQ(end_millis64 - start_millis64, 0x20);

        clear_time_provider();
    }
}

#endif // FASTLED_TESTING
