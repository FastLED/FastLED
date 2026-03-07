#include "fl/stl/async.h"
#include "fl/stl/task.h"
#include "fl/promise.h"
#include "fl/stl/new.h"
#include "test.h"
#include "fl/promise_result.h"
#include "fl/stl/function.h"
#include "fl/stl/move.h"
#include "fl/stl/string.h"
#include "fl/stl/thread.h"
#include "fl/stl/mutex.h"
#include "fl/math_macros.h"
#include "fl/stl/stdint.h"
#include "fl/stl/type_traits.h"
#include "fl/stl/utility.h"
#include "fl/stl/atomic.h"
#include "fl/delay.h"
#include "mutex_stub_stl.h"
#include "thread_stub_stl.h"
#include "fl/stl/vector.h"

FL_TEST_FILE(FL_FILEPATH) {

using namespace fl;

// Test helper: A simple test async runner
class TestAsyncRunner : public fl::async_runner {
public:
    TestAsyncRunner() : update_count(0), active(false), task_count(0) {}

    void update() override {
        update_count++;
    }

    bool has_active_tasks() const override {
        return active;
    }

    size_t active_task_count() const override {
        return task_count;
    }

    void set_active(bool value) { active = value; }
    void set_task_count(size_t count) { task_count = count; }

    int update_count;

private:
    bool active;
    size_t task_count;
};

FL_TEST_CASE("fl::async_runner interface") {
    FL_SUBCASE("basic implementation") {
        TestAsyncRunner runner;

        FL_CHECK_EQ(runner.update_count, 0);
        FL_CHECK_EQ(runner.has_active_tasks(), false);
        FL_CHECK_EQ(runner.active_task_count(), 0);

        runner.update();
        FL_CHECK_EQ(runner.update_count, 1);

        runner.set_active(true);
        runner.set_task_count(5);
        FL_CHECK_EQ(runner.has_active_tasks(), true);
        FL_CHECK_EQ(runner.active_task_count(), 5);
    }
}

FL_TEST_CASE("fl::AsyncManager") {
    FL_SUBCASE("singleton instance") {
        AsyncManager& mgr1 = AsyncManager::instance();
        AsyncManager& mgr2 = AsyncManager::instance();
        FL_CHECK_EQ(&mgr1, &mgr2);
    }

    FL_SUBCASE("register and unregister runners") {
        AsyncManager& mgr = AsyncManager::instance();
        TestAsyncRunner runner1;
        TestAsyncRunner runner2;

        // Register runners
        mgr.register_runner(&runner1);
        mgr.register_runner(&runner2);

        // Verify they're registered by calling update
        mgr.update_all();
        FL_CHECK_EQ(runner1.update_count, 1);
        FL_CHECK_EQ(runner2.update_count, 1);

        // Unregister one
        mgr.unregister_runner(&runner1);
        mgr.update_all();
        FL_CHECK_EQ(runner1.update_count, 1); // Not updated again
        FL_CHECK_EQ(runner2.update_count, 2); // Updated again

        // Cleanup
        mgr.unregister_runner(&runner2);
    }

    FL_SUBCASE("duplicate registration") {
        AsyncManager& mgr = AsyncManager::instance();
        TestAsyncRunner runner;

        // Register same runner multiple times
        mgr.register_runner(&runner);
        mgr.register_runner(&runner);
        mgr.register_runner(&runner);

        // Should only be registered once
        mgr.update_all();
        FL_CHECK_EQ(runner.update_count, 1);

        // Cleanup
        mgr.unregister_runner(&runner);
    }

    FL_SUBCASE("null runner handling") {
        AsyncManager& mgr = AsyncManager::instance();

        // Should handle null gracefully
        mgr.register_runner(nullptr);
        mgr.unregister_runner(nullptr);
        mgr.update_all(); // Should not crash
    }

    FL_SUBCASE("has_active_tasks") {
        AsyncManager& mgr = AsyncManager::instance();
        TestAsyncRunner runner1;
        TestAsyncRunner runner2;

        mgr.register_runner(&runner1);
        mgr.register_runner(&runner2);

        runner1.set_active(false);
        runner2.set_active(false);
        FL_CHECK_EQ(mgr.has_active_tasks(), false);

        runner1.set_active(true);
        FL_CHECK_EQ(mgr.has_active_tasks(), true);

        runner1.set_active(false);
        runner2.set_active(true);
        FL_CHECK_EQ(mgr.has_active_tasks(), true);

        // Cleanup
        mgr.unregister_runner(&runner1);
        mgr.unregister_runner(&runner2);
    }

    FL_SUBCASE("total_active_tasks") {
        AsyncManager& mgr = AsyncManager::instance();
        TestAsyncRunner runner1;
        TestAsyncRunner runner2;

        mgr.register_runner(&runner1);
        mgr.register_runner(&runner2);

        runner1.set_task_count(3);
        runner2.set_task_count(5);

        FL_CHECK_EQ(mgr.total_active_tasks(), 8);

        runner1.set_task_count(0);
        FL_CHECK_EQ(mgr.total_active_tasks(), 5);

        // Cleanup
        mgr.unregister_runner(&runner1);
        mgr.unregister_runner(&runner2);
    }
}

FL_TEST_CASE("fl::async_run") {
    FL_SUBCASE("updates all registered runners") {
        AsyncManager& mgr = AsyncManager::instance();
        TestAsyncRunner runner;

        mgr.register_runner(&runner);

        FL_CHECK_EQ(runner.update_count, 0);
        async_run();
        FL_CHECK_EQ(runner.update_count, 1);
        async_run();
        FL_CHECK_EQ(runner.update_count, 2);

        // Cleanup
        mgr.unregister_runner(&runner);
    }
}

FL_TEST_CASE("fl::async_active_tasks") {
    FL_SUBCASE("returns total active tasks") {
        AsyncManager& mgr = AsyncManager::instance();
        TestAsyncRunner runner1;
        TestAsyncRunner runner2;

        mgr.register_runner(&runner1);
        mgr.register_runner(&runner2);

        runner1.set_task_count(2);
        runner2.set_task_count(3);

        FL_CHECK_EQ(async_active_tasks(), 5);

        // Cleanup
        mgr.unregister_runner(&runner1);
        mgr.unregister_runner(&runner2);
    }
}

FL_TEST_CASE("fl::async_has_tasks") {
    FL_SUBCASE("checks for any active tasks") {
        AsyncManager& mgr = AsyncManager::instance();
        TestAsyncRunner runner;

        mgr.register_runner(&runner);

        runner.set_active(false);
        FL_CHECK_EQ(async_has_tasks(), false);

        runner.set_active(true);
        FL_CHECK_EQ(async_has_tasks(), true);

        // Cleanup
        mgr.unregister_runner(&runner);
    }
}

FL_TEST_CASE("fl::async_yield") {
    FL_SUBCASE("pumps async tasks") {
        AsyncManager& mgr = AsyncManager::instance();
        TestAsyncRunner runner;

        mgr.register_runner(&runner);

        FL_CHECK_EQ(runner.update_count, 0);
        async_yield();
        // async_yield calls async_run at least once, plus additional pumps
        FL_CHECK(runner.update_count >= 1);

        // Cleanup
        mgr.unregister_runner(&runner);
    }
}

FL_TEST_CASE("fl::await_top_level - Basic Operations") {
    FL_SUBCASE("await_top_level resolved promise returns value") {
        auto promise = fl::promise<int>::resolve(42);
        auto result = fl::await_top_level(promise);  // Type automatically deduced!

        FL_CHECK(result.ok());
        FL_CHECK_EQ(result.value(), 42);
    }

    FL_SUBCASE("await_top_level rejected promise returns error") {
        auto promise = fl::promise<int>::reject(fl::Error("Test error"));
        auto result = fl::await_top_level(promise);  // Type automatically deduced!

        FL_CHECK(!result.ok());
        FL_CHECK_EQ(result.error().message, "Test error");
    }

    FL_SUBCASE("await_top_level invalid promise returns error") {
        fl::promise<int> invalid_promise; // Default constructor creates invalid promise
        auto result = fl::await_top_level(invalid_promise);  // Type automatically deduced!

        FL_CHECK(!result.ok());
        FL_CHECK_EQ(result.error().message, "Invalid promise");
    }

    FL_SUBCASE("explicit template parameter still works") {
        auto promise = fl::promise<int>::resolve(42);
        auto result = fl::await_top_level<int>(promise);  // Explicit template parameter

        FL_CHECK(result.ok());
        FL_CHECK_EQ(result.value(), 42);
    }
}

FL_TEST_CASE("fl::await_top_level - Asynchronous Completion") {
    FL_SUBCASE("await_top_level waits for promise to be resolved") {
        auto promise = fl::promise<int>::create();
        bool promise_completed = false;

        // Simulate async completion in background
        // Note: In a real scenario, this would be done by an async system
        // For testing, we'll complete it immediately after starting await_top_level

        // Complete the promise with a value
        promise.complete_with_value(123);
        promise_completed = true;

        auto result = fl::await_top_level(promise);  // Type automatically deduced!

        FL_CHECK(promise_completed);
        FL_CHECK(result.ok());
        FL_CHECK_EQ(result.value(), 123);
    }

    FL_SUBCASE("await_top_level waits for promise to be rejected") {
        auto promise = fl::promise<int>::create();
        bool promise_completed = false;

        // Complete the promise with an error
        promise.complete_with_error(fl::Error("Async error"));
        promise_completed = true;

        auto result = fl::await_top_level(promise);  // Type automatically deduced!

        FL_CHECK(promise_completed);
        FL_CHECK(!result.ok());
        FL_CHECK_EQ(result.error().message, "Async error");
    }
}

FL_TEST_CASE("fl::await_top_level - Different Value Types") {
    FL_SUBCASE("await_top_level with string type") {
        auto promise = fl::promise<fl::string>::resolve(fl::string("Hello, World!"));
        auto result = fl::await_top_level(promise);  // Type automatically deduced!

        FL_CHECK(result.ok());
        FL_CHECK_EQ(result.value(), "Hello, World!");
    }

    FL_SUBCASE("await_top_level with custom struct") {
        struct TestData {
            int x;
            fl::string name;

            bool operator==(const TestData& other) const {
                return x == other.x && name == other.name;
            }
        };

        TestData expected{42, "test"};
        auto promise = fl::promise<TestData>::resolve(expected);
        auto result = fl::await_top_level(promise);  // Type automatically deduced!

        FL_CHECK(result.ok());
        FL_CHECK(result.value() == expected);
    }
}

FL_TEST_CASE("fl::await_top_level - Error Handling") {
    FL_SUBCASE("await_top_level preserves error message") {
        fl::string error_msg = "Detailed error message";
        auto promise = fl::promise<int>::reject(fl::Error(error_msg));
        auto result = fl::await_top_level(promise);  // Type automatically deduced!

        FL_CHECK(!result.ok());
        FL_CHECK_EQ(result.error().message, error_msg);
    }

    FL_SUBCASE("await_top_level with custom error") {
        fl::Error custom_error("Custom error with details");
        auto promise = fl::promise<fl::string>::reject(custom_error);
        auto result = fl::await_top_level(promise);  // Type automatically deduced!

        FL_CHECK(!result.ok());
        FL_CHECK_EQ(result.error().message, "Custom error with details");
    }
}

FL_TEST_CASE("fl::await_top_level - Multiple Awaits") {
    FL_SUBCASE("multiple awaits on different promises") {
        auto promise1 = fl::promise<int>::resolve(10);
        auto promise2 = fl::promise<int>::resolve(20);
        auto promise3 = fl::promise<int>::reject(fl::Error("Error in promise 3"));

        auto result1 = fl::await_top_level(promise1);  // Type automatically deduced!
        auto result2 = fl::await_top_level(promise2);  // Type automatically deduced!
        auto result3 = fl::await_top_level(promise3);  // Type automatically deduced!

        // Check first result
        FL_CHECK(result1.ok());
        FL_CHECK_EQ(result1.value(), 10);

        // Check second result
        FL_CHECK(result2.ok());
        FL_CHECK_EQ(result2.value(), 20);

        // Check third result (error)
        FL_CHECK(!result3.ok());
        FL_CHECK_EQ(result3.error().message, "Error in promise 3");
    }

    FL_SUBCASE("await_top_level same promise multiple times") {
        auto promise = fl::promise<int>::resolve(999);

        auto result1 = fl::await_top_level(promise);  // Type automatically deduced!
        auto result2 = fl::await_top_level(promise);  // Type automatically deduced!

        // Both awaits should return the same result
        FL_CHECK(result1.ok());
        FL_CHECK(result2.ok());

        FL_CHECK_EQ(result1.value(), 999);
        FL_CHECK_EQ(result2.value(), 999);
    }
}

FL_TEST_CASE("fl::await_top_level - Boolean Conversion and Convenience") {
    FL_SUBCASE("boolean conversion operator") {
        auto success_promise = fl::promise<int>::resolve(42);
        auto success_result = fl::await_top_level(success_promise);

        auto error_promise = fl::promise<int>::reject(fl::Error("Error"));
        auto error_result = fl::await_top_level(error_promise);

        // Test boolean conversion (should work like ok())
        FL_CHECK(success_result);   // Implicit conversion to bool
        FL_CHECK(!error_result);    // Implicit conversion to bool

        // Equivalent to ok() method
        FL_CHECK(success_result.ok());
        FL_CHECK(!error_result.ok());
    }

    FL_SUBCASE("error_message convenience method") {
        auto success_promise = fl::promise<int>::resolve(42);
        auto success_result = fl::await_top_level(success_promise);

        auto error_promise = fl::promise<int>::reject(fl::Error("Test error"));
        auto error_result = fl::await_top_level(error_promise);

        // Test error_message convenience method
        FL_CHECK_EQ(success_result.error_message(), "");  // Empty string for success
        FL_CHECK_EQ(error_result.error_message(), "Test error");  // Error message for failure
    }
}

FL_TEST_CASE("fl::Scheduler") {
    FL_SUBCASE("singleton instance") {
        Scheduler& sched1 = Scheduler::instance();
        Scheduler& sched2 = Scheduler::instance();
        FL_CHECK_EQ(&sched1, &sched2);
    }

    FL_SUBCASE("add_task returns task id") {
        Scheduler& sched = Scheduler::instance();
        sched.clear_all_tasks();

        bool executed = false;
        auto t = fl::task::every_ms(1).then([&executed]() {
            executed = true;
        });

        int task_id = sched.add_task(fl::move(t));
        FL_CHECK(task_id > 0);

        sched.clear_all_tasks();
    }

    FL_SUBCASE("update executes ready tasks") {
        Scheduler& sched = Scheduler::instance();
        sched.clear_all_tasks();

        bool executed = false;
        auto t = fl::task::every_ms(0).then([&executed]() {
            executed = true;
        });

        sched.add_task(fl::move(t));

        FL_CHECK_EQ(executed, false);
        sched.update();
        FL_CHECK_EQ(executed, true);

        sched.clear_all_tasks();
    }

    FL_SUBCASE("clear_all_tasks") {
        Scheduler& sched = Scheduler::instance();
        sched.clear_all_tasks();

        bool executed = false;
        auto t = fl::task::every_ms(1000).then([&executed]() {
            executed = true;
        });

        sched.add_task(fl::move(t));
        sched.clear_all_tasks();
        sched.update();

        FL_CHECK_EQ(executed, false); // Task was cleared before execution
    }

    FL_SUBCASE("update_before_frame_tasks") {
        Scheduler& sched = Scheduler::instance();
        sched.clear_all_tasks();

        bool before_executed = false;
        bool after_executed = false;

        auto before_task = fl::task::before_frame().then([&before_executed]() {
            before_executed = true;
        });

        auto after_task = fl::task::after_frame().then([&after_executed]() {
            after_executed = true;
        });

        sched.add_task(fl::move(before_task));
        sched.add_task(fl::move(after_task));

        sched.update_before_frame_tasks();
        FL_CHECK_EQ(before_executed, true);
        FL_CHECK_EQ(after_executed, false);

        sched.update_after_frame_tasks();
        FL_CHECK_EQ(after_executed, true);

        sched.clear_all_tasks();
    }

    FL_SUBCASE("update_after_frame_tasks") {
        Scheduler& sched = Scheduler::instance();
        sched.clear_all_tasks();

        bool executed = false;
        auto t = fl::task::after_frame().then([&executed]() {
            executed = true;
        });

        sched.add_task(fl::move(t));

        FL_CHECK_EQ(executed, false);
        sched.update_after_frame_tasks();
        FL_CHECK_EQ(executed, true);

        sched.clear_all_tasks();
    }
}

FL_TEST_CASE("fl::async integration") {
    FL_SUBCASE("scheduler tasks work with async_run") {
        Scheduler& sched = Scheduler::instance();
        sched.clear_all_tasks();

        bool executed = false;
        auto t = fl::task::every_ms(0).then([&executed]() {
            executed = true;
        });

        sched.add_task(fl::move(t));

        FL_CHECK_EQ(executed, false);
        async_run(); // Should update scheduler and async manager
        FL_CHECK_EQ(executed, true);

        sched.clear_all_tasks();
    }
}

// ============================================================
// Coroutine-based await tests (merged from await_coroutine.cpp)
// ============================================================

#ifdef FASTLED_STUB_IMPL
#endif

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
        for (uint32_t elapsed = 0; elapsed < delay_ms && !g_shutdown_requested.load(); elapsed += 1) {
            uint32_t sleep_time = fl::min(1u, delay_ms - elapsed);
            fl::this_thread::sleep_for(fl::chrono::milliseconds(sleep_time));  // ok sleep for
        }

        // Only complete if not shutting down
        if (!g_shutdown_requested.load()) {
            // Complete promise WITHOUT acquiring global lock
            // The promise callbacks (cv.notify) can run from any thread
            p.complete_with_value(value);
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
        for (uint32_t elapsed = 0; elapsed < delay_ms && !g_shutdown_requested.load(); elapsed += 1) {
            uint32_t sleep_time = fl::min(1u, delay_ms - elapsed);
            fl::this_thread::sleep_for(fl::chrono::milliseconds(sleep_time));  // ok sleep for
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

FL_TEST_CASE("await in coroutine - basic resolution") {
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
        FL_CHECK(result.ok());
        if (result.ok()) {
            result_value.store(result.value());
        }

        test_completed.store(true);
    };
    config.name = "TestAwait";
    auto coro = task::coroutine(config);

    // Wait for coroutine to complete (max 200ms to account for slow CI)
    int timeout = 0;
    while (!test_completed.load() && timeout < 200) {
        async_yield();  // Release lock and pump async tasks
        delay(5);
        timeout += 5;
    }

    FL_CHECK(test_completed.load());
    FL_CHECK(result_value.load() == 42);

    // Clean up background threads before test exits
    cleanup_threads();
}

FL_TEST_CASE("await in coroutine - error handling") {
    fl::atomic<bool> test_completed(false);
    fl::atomic<bool> got_error(false);

    CoroutineConfig config;
    config.function = [&]() {
        // Create promise that rejects after 5ms (reduced from 50ms)
        auto p = delayed_reject<int>(Error("Test error"), 5);

        // Await the promise
        auto result = fl::await(p);

        // Should have error
        FL_CHECK(!result.ok());
        if (!result.ok()) {
            got_error.store(true);
        }

        test_completed.store(true);
    };
    config.name = "TestAwaitError";
    auto coro = task::coroutine(config);

    // Wait for completion (max 200ms to account for slow CI)
    int timeout = 0;
    while (!test_completed.load() && timeout < 200) {
        async_yield();
        delay(5);
        timeout += 5;
    }

    FL_CHECK(test_completed.load());
    FL_CHECK(got_error.load());

    // Clean up background threads before test exits
    cleanup_threads();
}

FL_TEST_CASE("await in coroutine - already completed promise") {
    fl::atomic<bool> test_completed(false);
    fl::atomic<int> result_value(0);

    CoroutineConfig config;
    config.function = [&]() {
        // Create already-resolved promise
        auto p = promise<int>::resolve(123);

        // Await should return immediately
        auto result = fl::await(p);

        FL_CHECK(result.ok());
        if (result.ok()) {
            result_value.store(result.value());
        }

        test_completed.store(true);
    };
    config.name = "TestAwaitImmediate";
    auto coro = task::coroutine(config);

    // Should complete quickly (within 200ms to account for slow CI)
    int timeout = 0;
    while (!test_completed.load() && timeout < 200) {
        async_yield();
        delay(5);
        timeout += 5;
    }

    FL_CHECK(test_completed.load());
    FL_CHECK(result_value.load() == 123);
}

FL_TEST_CASE("await in coroutine - multiple concurrent coroutines") {
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
            // Each promise resolves to i*10 after (i*2)ms
            auto p = delayed_resolve<int>(i * 10, i * 2);
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

    // Wait for all coroutines to complete (max 500ms to account for slow CI)
    int timeout = 0;
    printf("Test: Waiting for coroutines to complete...\n");
    while (completed_count.load() < 5 && timeout < 500) {
        if (timeout % 100 == 0) {
            printf("Test: timeout=%d, completed=%d, sum=%d\n", timeout, completed_count.load(), sum.load());
        }
        async_yield();
        delay(5);
        timeout += 5;
    }
    printf("Test: Wait complete. completed=%d, sum=%d\n", completed_count.load(), sum.load());

    FL_CHECK(completed_count.load() == 5);
    FL_CHECK(sum.load() == 0 + 10 + 20 + 30 + 40);  // Sum of 0, 10, 20, 30, 40

    // Clean up background threads before test exits
    cleanup_threads();
}

FL_TEST_CASE("await in coroutine - invalid promise") {
    fl::atomic<bool> test_completed(false);
    fl::atomic<bool> got_error(false);

    CoroutineConfig config;
    config.function = [&]() {
        // Create invalid promise (default constructor)
        promise<int> p;

        // Await should return error immediately
        auto result = fl::await(p);

        FL_CHECK(!result.ok());
        if (!result.ok()) {
            got_error.store(true);
        }

        test_completed.store(true);
    };
    config.name = "TestAwaitInvalid";
    auto coro = task::coroutine(config);

    // Should complete quickly (within 200ms to account for slow CI)
    int timeout = 0;
    while (!test_completed.load() && timeout < 200) {
        async_yield();
        delay(5);
        timeout += 5;
    }

    FL_CHECK(test_completed.load());
    FL_CHECK(got_error.load());
}

FL_TEST_CASE("await in coroutine - sequential awaits") {
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

    // Wait for completion (max 200ms to account for slow CI)
    int timeout = 0;
    while (!test_completed.load() && timeout < 200) {
        async_yield();
        delay(5);
        timeout += 5;
    }

    FL_CHECK(test_completed.load());
    FL_CHECK(total.load() == 60);  // 10 + 20 + 30

    // Clean up background threads before test exits
    cleanup_threads();
}

FL_TEST_CASE("await vs await_top_level - CPU usage comparison") {
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
        FL_CHECK(result.ok());
        if (result.ok()) {
            FL_CHECK(result.value() == 42);
        }
        await_completed.store(true);
    };
    config.name = "TestAwaitBlocking";
    auto coro = task::coroutine(config);

    // Wait for coroutine to complete
    int timeout = 0;
    while (!await_completed.load() && timeout < 200) {
        async_yield();
        delay(5);
        timeout += 5;
    }
    FL_CHECK(await_completed.load());

    // Clean up background threads before test exits
    cleanup_threads();

    // Note: await_top_level requires integration with the async system
    // which is not set up in this unit test environment. The above test
    // verifies that await() works correctly in coroutines.
}

#ifdef FASTLED_STUB_IMPL
FL_TEST_CASE("global coordination - no concurrent execution") {
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
    while (!test_completed.load() && timeout < 500) {
        async_yield();  // Keep yielding to let coroutine finish
        delay(1);
        timeout += 1;
    }

    // Verify no race condition detected
    FL_CHECK(test_completed.load());
    FL_CHECK_FALSE(race_detected.load());
    FL_CHECK(max_concurrent_threads.load() == 1);  // Only one thread at a time
}

FL_TEST_CASE("global coordination - await releases lock for other threads") {
    // This test verifies that when a coroutine calls await(), it releases
    // the global lock, allowing other threads to make progress

    fl::atomic<int> coroutine1_progress(0);
    fl::atomic<int> coroutine2_progress(0);

    // Spawn two coroutines that await different promises
    CoroutineConfig config1;
    config1.function = [&]() {
        coroutine1_progress.store(1);  // Started

        // Await a promise (should release global lock) - reduced from 100ms to 10ms
        auto p = delayed_resolve<int>(42, 10);
        auto result = fl::await(p);

        coroutine1_progress.store(2);  // Completed

        // Check if coroutine 2 made progress while we were awaiting
        FL_CHECK(coroutine2_progress.load() >= 1);
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
        FL_CHECK(coroutine1_progress.load() >= 1);
    };
    config2.name = "TestCoro2";
    auto coro2 = task::coroutine(config2);

    // Wait for BOTH coroutines to actually complete (progress == 2)
    // This avoids race condition where one coroutine finishes before the other
    int timeout = 0;
    while ((coroutine1_progress.load() < 2 || coroutine2_progress.load() < 2) && timeout < 500) {
        async_yield();
        delay(1);
        timeout += 1;
    }

    FL_CHECK(coroutine1_progress.load() == 2);
    FL_CHECK(coroutine2_progress.load() == 2);

    // Clean up background threads before test exits
    cleanup_threads();
}
#endif // FASTLED_STUB_IMPL

} // FL_TEST_FILE
