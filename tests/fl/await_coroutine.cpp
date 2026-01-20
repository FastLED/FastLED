#include "fl/async.h"
#include "fl/task.h"
#include "fl/promise.h"
#include "fl/stl/thread.h"
#include "fl/stl/mutex.h"
#include "fl/math_macros.h"
#include "fl/stl/stdint.h"
#include "fl/str.h" // ok include
#include "fl/stl/thread.h"
#include "fl/stl/new.h"
#include "fl/stl/type_traits.h"
#include "fl/stl/utility.h"
#include "doctest.h"
#include "fl/promise_result.h"
#include "fl/stl/atomic.h"
#include "fl/stl/function.h"
#include "fl/stl/move.h"
#include "fl/stl/string.h"
#include "mutex_stub_stl.h"
#include "thread_stub_stl.h"
#include "fl/stl/vector.h"

#ifdef FASTLED_STUB_IMPL
#endif

using namespace fl;

// Global thread registry for proper cleanup in DLL mode
// When tests complete, we need to join all background threads before DLL unload
namespace {
    fl::vector<fl::thread> g_background_threads;
    fl::atomic<bool> g_shutdown_requested(false);
    fl::mutex g_threads_mutex;

    void register_thread(fl::thread&& t) {
        fl::unique_lock<fl::mutex> lock(g_threads_mutex);
        g_background_threads.push_back(fl::move(t));
    }

    void cleanup_threads() {
        g_shutdown_requested.store(true);
        fl::unique_lock<fl::mutex> lock(g_threads_mutex);
        for (auto& t : g_background_threads) {
            if (t.joinable()) {
                t.join();
            }
        }
        g_background_threads.clear();
        g_shutdown_requested.store(false);
    }
}

// Helper: Create a promise that resolves after a delay
// Note: The background thread does NOT acquire the global lock!
// This simulates external events (ISR, network callbacks, etc.) that complete promises
// Thread is registered for proper cleanup on test exit
template<typename T>
promise<T> delayed_resolve(const T& value, uint32_t delay_ms) {
    auto p = promise<T>::create();

    // Simulate async work in a background thread
    // This thread does NOT participate in global coordination (like ISR)
    fl::thread t([p, value, delay_ms]() mutable {
        // Sleep in small increments to allow early exit on shutdown
        for (uint32_t elapsed = 0; elapsed < delay_ms && !g_shutdown_requested.load(); elapsed += 10) {
            uint32_t sleep_time = FL_MIN(10u, delay_ms - elapsed);
            fl::this_thread::sleep_for(std::chrono::milliseconds(sleep_time)); // okay std namespace
        }

        // Only complete if not shutting down
        if (!g_shutdown_requested.load()) {
            printf("    [Background] Promise resolving with value %d after %u ms\n", (int)value, delay_ms);
            // Complete promise WITHOUT acquiring global lock
            // The promise callbacks (cv.notify) can run from any thread
            p.complete_with_value(value);
            printf("    [Background] Promise completed with value %d\n", (int)value);
        }
    });

    register_thread(fl::move(t));
    return p;
}

// Helper: Create a promise that rejects after a delay
template<typename T>
promise<T> delayed_reject(const Error& error, uint32_t delay_ms) {
    auto p = promise<T>::create();

    fl::thread t([p, error, delay_ms]() mutable {
        // Sleep in small increments to allow early exit on shutdown
        for (uint32_t elapsed = 0; elapsed < delay_ms && !g_shutdown_requested.load(); elapsed += 10) {
            uint32_t sleep_time = FL_MIN(10u, delay_ms - elapsed);
            fl::this_thread::sleep_for(std::chrono::milliseconds(sleep_time)); // okay std namespace
        }

        // Only complete if not shutting down
        if (!g_shutdown_requested.load()) {
            // Complete promise WITHOUT acquiring global lock
            p.complete_with_error(error);
        }
    });

    register_thread(fl::move(t));
    return p;
}

TEST_CASE("await in coroutine - basic resolution") {
    fl::atomic<bool> test_completed(false);
    fl::atomic<int> result_value(0);

    // Spawn coroutine that awaits a promise
    CoroutineConfig config;
    config.function = [&]() {
        // Create promise that resolves to 42 after 5ms (reduced from 50ms)
        auto p = delayed_resolve<int>(42, 5);

        // Await the promise (should block this coroutine only)
        auto result = fl::await(p);

        // Verify result
        CHECK(result.ok());
        if (result.ok()) {
            result_value.store(result.value());
        }

        test_completed.store(true);
    };
    config.name = "TestAwait";
    auto coro = task::coroutine(config);

    // Wait for coroutine to complete (max 200ms, reduced from 2000ms)
    int timeout = 0;
    while (!test_completed.load() && timeout < 200) {
        async_yield();  // Release lock and pump async tasks
        timeout += 10;
    }

    CHECK(test_completed.load());
    CHECK(result_value.load() == 42);

    // Clean up background threads before test exits
    cleanup_threads();
}

TEST_CASE("await in coroutine - error handling") {
    fl::atomic<bool> test_completed(false);
    fl::atomic<bool> got_error(false);

    CoroutineConfig config;
    config.function = [&]() {
        // Create promise that rejects after 5ms (reduced from 50ms)
        auto p = delayed_reject<int>(Error("Test error"), 5);

        // Await the promise
        auto result = fl::await(p);

        // Should have error
        CHECK(!result.ok());
        if (!result.ok()) {
            got_error.store(true);
        }

        test_completed.store(true);
    };
    config.name = "TestAwaitError";
    auto coro = task::coroutine(config);

    // Wait for completion
    int timeout = 0;
    while (!test_completed.load() && timeout < 200) {
        async_yield(); fl::this_thread::sleep_for(std::chrono::milliseconds(1)); // okay std namespace // Yield and give time
        timeout += 10;
    }

    CHECK(test_completed.load());
    CHECK(got_error.load());

    // Clean up background threads before test exits
    cleanup_threads();
}

TEST_CASE("await in coroutine - already completed promise") {
    fl::atomic<bool> test_completed(false);
    fl::atomic<int> result_value(0);

    CoroutineConfig config;
    config.function = [&]() {
        // Create already-resolved promise
        auto p = promise<int>::resolve(123);

        // Await should return immediately
        auto result = fl::await(p);

        CHECK(result.ok());
        if (result.ok()) {
            result_value.store(result.value());
        }

        test_completed.store(true);
    };
    config.name = "TestAwaitImmediate";
    auto coro = task::coroutine(config);

    // Should complete quickly (within 100ms)
    int timeout = 0;
    while (!test_completed.load() && timeout < 100) {
        async_yield(); fl::this_thread::sleep_for(std::chrono::milliseconds(1)); // okay std namespace // Yield and give time
        timeout += 10;
    }

    CHECK(test_completed.load());
    CHECK(result_value.load() == 123);
}

TEST_CASE("await in coroutine - multiple concurrent coroutines") {
    fl::atomic<int> completed_count(0);
    fl::atomic<int> sum(0);

    printf("Test: Spawning 5 coroutines\n");

    // Store task objects to keep coroutines alive
    fl::vector<task> tasks;

    // Spawn 5 coroutines, each awaiting different promises
    for (int i = 0; i < 5; i++) {
        CoroutineConfig config;
        config.function = [&, i]() {
            printf("  Coroutine %d: Started\n", i);
            // Each promise resolves to i*10 after (i*10)ms
            auto p = delayed_resolve<int>(i * 10, i * 10);
            printf("  Coroutine %d: Created promise, calling await()\n", i);
            auto result = fl::await(p);
            printf("  Coroutine %d: await() returned, ok=%d\n", i, result.ok());

            if (result.ok()) {
                printf("  Coroutine %d: Adding value %d to sum\n", i, result.value());
                sum.fetch_add(result.value());
            }

            completed_count.fetch_add(1);
            printf("  Coroutine %d: Completed\n", i);
        };
        config.name = fl::string("TestCoro") + fl::to_string(i);
        tasks.push_back(task::coroutine(config));
        printf("Test: Spawned coroutine %d\n", i);
    }

    // Wait for all coroutines to complete
    int timeout = 0;
    printf("Test: Waiting for coroutines to complete...\n");
    while (completed_count.load() < 5 && timeout < 2000) {
        if (timeout % 200 == 0) {
            printf("Test: timeout=%d, completed=%d, sum=%d\n", timeout, completed_count.load(), sum.load());
        }
        async_yield(); fl::this_thread::sleep_for(std::chrono::milliseconds(1)); // okay std namespace // Yield and give time
        timeout += 10;
    }
    printf("Test: Wait complete. completed=%d, sum=%d\n", completed_count.load(), sum.load());

    CHECK(completed_count.load() == 5);
    CHECK(sum.load() == 0 + 10 + 20 + 30 + 40);  // Sum of 0, 10, 20, 30, 40

    // Clean up background threads before test exits
    cleanup_threads();
}

TEST_CASE("await in coroutine - invalid promise") {
    fl::atomic<bool> test_completed(false);
    fl::atomic<bool> got_error(false);

    CoroutineConfig config;
    config.function = [&]() {
        // Create invalid promise (default constructor)
        promise<int> p;

        // Await should return error immediately
        auto result = fl::await(p);

        CHECK(!result.ok());
        if (!result.ok()) {
            got_error.store(true);
        }

        test_completed.store(true);
    };
    config.name = "TestAwaitInvalid";
    auto coro = task::coroutine(config);

    // Should complete quickly
    int timeout = 0;
    while (!test_completed.load() && timeout < 100) {
        async_yield(); fl::this_thread::sleep_for(std::chrono::milliseconds(1)); // okay std namespace // Yield and give time
        timeout += 10;
    }

    CHECK(test_completed.load());
    CHECK(got_error.load());
}

TEST_CASE("await in coroutine - sequential awaits") {
    fl::atomic<bool> test_completed(false);
    fl::atomic<int> total(0);

    CoroutineConfig config;
    config.function = [&]() {
        // First await (reduced from 20ms to 2ms)
        auto p1 = delayed_resolve<int>(10, 2);
        auto r1 = fl::await(p1);
        if (r1.ok()) total.fetch_add(r1.value());

        // Second await (reduced from 20ms to 2ms)
        auto p2 = delayed_resolve<int>(20, 2);
        auto r2 = fl::await(p2);
        if (r2.ok()) total.fetch_add(r2.value());

        // Third await (reduced from 20ms to 2ms)
        auto p3 = delayed_resolve<int>(30, 2);
        auto r3 = fl::await(p3);
        if (r3.ok()) total.fetch_add(r3.value());

        test_completed.store(true);
    };
    config.name = "TestAwaitSequential";
    auto coro = task::coroutine(config);

    // Wait for completion (should take ~6ms + overhead, reduced from 2000ms)
    int timeout = 0;
    while (!test_completed.load() && timeout < 200) {
        async_yield(); fl::this_thread::sleep_for(std::chrono::milliseconds(1)); // okay std namespace // Yield and give time
        timeout += 10;
    }

    CHECK(test_completed.load());
    CHECK(total.load() == 60);  // 10 + 20 + 30

    // Clean up background threads before test exits
    cleanup_threads();
}

TEST_CASE("await vs await_top_level - CPU usage comparison") {
    // This test demonstrates the difference between await (blocking)
    // and await_top_level (busy-wait)
    //
    // NOTE: This test primarily verifies that both functions work correctly.
    // The CPU usage difference (await = efficient blocking, await_top_level = busy-wait)
    // is not directly testable in unit tests but can be observed with profiling tools.

    fl::atomic<bool> await_completed(false);

    // Test 1: await in coroutine (should NOT busy-wait)
    CoroutineConfig config;
    config.function = [&]() {
        auto p = delayed_resolve<int>(42, 5);  // 5ms delay (reduced from 50ms)
        auto result = fl::await(p);
        CHECK(result.ok());
        if (result.ok()) {
            CHECK(result.value() == 42);
        }
        await_completed.store(true);
    };
    config.name = "TestAwaitBlocking";
    auto coro = task::coroutine(config);

    // Wait for coroutine to complete
    int timeout = 0;
    while (!await_completed.load() && timeout < 200) {
        async_yield(); fl::this_thread::sleep_for(std::chrono::milliseconds(1)); // okay std namespace // Yield and give time
        timeout += 10;
    }
    CHECK(await_completed.load());

    // Clean up background threads before test exits
    cleanup_threads();

    // Note: await_top_level requires integration with the async system
    // which is not set up in this unit test environment. The above test
    // verifies that await() works correctly in coroutines.
}

#ifdef FASTLED_STUB_IMPL
TEST_CASE("global coordination - no concurrent execution") {
    // This test verifies that the global execution lock prevents
    // the main thread and coroutine threads from executing simultaneously
    //
    // The main thread holds the global lock (acquired in FL_INIT)
    // Coroutines also acquire the lock on startup
    // This test verifies mutual exclusion works properly

    fl::atomic<int> active_threads(0);
    fl::atomic<int> max_concurrent_threads(0);
    fl::atomic<bool> race_detected(false);
    fl::atomic<bool> test_completed(false);

    // Spawn a coroutine that increments/decrements active counter
    CoroutineConfig config;
    config.function = [&]() {
        for (int i = 0; i < 50; i++) {
            // Increment active counter
            int before = active_threads.fetch_add(1);

            // Check if another thread is active (race condition!)
            if (before > 0) {
                race_detected.store(true);
            }

            // Update max concurrent threads
            int current = active_threads.load();
            int prev_max = max_concurrent_threads.load();
            while (current > prev_max &&
                   !max_concurrent_threads.compare_exchange_weak(prev_max, current)) {
                prev_max = max_concurrent_threads.load();
            }

            // Simulate some work (yield is inside coroutine, safe)
            fl::this_thread::yield();

            // Decrement active counter
            active_threads.fetch_sub(1);

            // Yield to allow main thread to run
            fl::this_thread::yield();
        }
        test_completed.store(true);
    };
    config.name = "TestRaceCondition";
    auto coro = task::coroutine(config);

    // Main thread also tests mutual exclusion by checking active_threads
    // while yielding to give coroutine chances to run
    for (int i = 0; i < 50 && !test_completed.load(); i++) {
        // Increment active counter
        int before = active_threads.fetch_add(1);

        // Check if another thread is active (race condition!)
        if (before > 0) {
            race_detected.store(true);
        }

        // Update max concurrent threads
        int current = active_threads.load();
        int prev_max = max_concurrent_threads.load();
        while (current > prev_max &&
               !max_concurrent_threads.compare_exchange_weak(prev_max, current)) {
            prev_max = max_concurrent_threads.load();
        }

        // Simulate some work (yield is in main thread context, safe)
        fl::this_thread::yield();

        // Decrement active counter
        active_threads.fetch_sub(1);

        // Yield to allow coroutine to run (releases global lock)
        async_yield();
    }

    // Wait for coroutine to complete
    int timeout = 0;
    while (!test_completed.load() && timeout < 5000) {
        async_yield();  // Keep yielding to let coroutine finish
        timeout += 10;
    }

    // Verify no race condition detected
    CHECK(test_completed.load());
    CHECK_FALSE(race_detected.load());
    CHECK(max_concurrent_threads.load() == 1);  // Only one thread at a time
}

TEST_CASE("global coordination - await releases lock for other threads") {
    // This test verifies that when a coroutine calls await(), it releases
    // the global lock, allowing other threads to make progress

    fl::atomic<int> coroutine1_progress(0);
    fl::atomic<int> coroutine2_progress(0);
    fl::atomic<bool> both_completed(false);

    // Spawn two coroutines that await different promises
    CoroutineConfig config1;
    config1.function = [&]() {
        coroutine1_progress.store(1);  // Started

        // Await a promise (should release global lock) - reduced from 100ms to 10ms
        auto p = delayed_resolve<int>(42, 10);
        auto result = fl::await(p);

        coroutine1_progress.store(2);  // Completed

        // Check if coroutine 2 made progress while we were awaiting
        CHECK(coroutine2_progress.load() >= 1);
    };
    config1.name = "TestCoro1";
    auto coro1 = task::coroutine(config1);

    CoroutineConfig config2;
    config2.function = [&]() {
        coroutine2_progress.store(1);  // Started

        // Await a different promise - reduced from 100ms to 10ms
        auto p = delayed_resolve<int>(99, 10);
        auto result = fl::await(p);

        coroutine2_progress.store(2);  // Completed

        // Check if coroutine 1 made progress
        CHECK(coroutine1_progress.load() >= 1);

        both_completed.store(true);
    };
    config2.name = "TestCoro2";
    auto coro2 = task::coroutine(config2);

    // Wait for both coroutines to complete (5000ms total timeout)
    int timeout = 0;
    while (!both_completed.load() && timeout < 5000) {
        async_yield(); fl::this_thread::sleep_for(std::chrono::milliseconds(1)); // okay std namespace // Yield and give time
        timeout += 1;  // Match the sleep duration
    }

    CHECK(both_completed.load());
    CHECK(coroutine1_progress.load() == 2);
    CHECK(coroutine2_progress.load() == 2);

    // Clean up background threads before test exits
    cleanup_threads();
}
#endif // FASTLED_STUB_IMPL
