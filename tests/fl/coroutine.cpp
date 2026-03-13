// ok cpp include
/// @file coroutine.cpp
/// @brief Unit tests for stub platform coroutine runner (independent of async)
///
/// Tests the coroutine system:
/// 1. Binary semaphore (lowest level primitive)
/// 2. task::coroutine (high-level API)

#include "test.h"
#include "fl/stl/task.h"
#include "fl/stl/atomic.h"
#include "fl/stl/thread.h"
#include "fl/stl/mutex.h"
#include "fl/stl/chrono.h"
#include "fl/stl/function.h"
#include "fl/stl/string.h"
#include "fl/stl/semaphore.h"
#include "fl/system/delay.h"
#include "fl/stl/async.h"
#include "platforms/coroutine.h"
#include "fl/engine_events.h"

FL_TEST_FILE(FL_FILEPATH) {

using namespace fl;

// ============================================================
// Level 1: Binary semaphore tests
// ============================================================

FL_TEST_CASE("coroutine - binary semaphore basic signal") {
    fl::binary_semaphore sem(0);
    FL_CHECK_FALSE(sem.try_acquire());
    sem.release();
    FL_CHECK(sem.try_acquire());
    FL_CHECK_FALSE(sem.try_acquire());
}

FL_TEST_CASE("coroutine - binary semaphore thread handoff") {
    fl::binary_semaphore sem(0);
    fl::atomic<bool> thread_ran(false);

    fl::thread t([&]() {
        sem.acquire();
        thread_ran.store(true);
    });

    fl::this_thread::sleep_for(fl::chrono::milliseconds(10)); // ok sleep for
    FL_CHECK_FALSE(thread_ran.load());

    sem.release();
    t.join();
    FL_CHECK(thread_ran.load());
}

FL_TEST_CASE("coroutine - counting semaphore two-phase handoff") {
    fl::counting_semaphore<2> sem(0);
    fl::atomic<int> phase(0);

    fl::thread t([&]() {
        phase.store(1);
        sem.release();  // Signal "started"

        // Simulate work
        fl::this_thread::sleep_for(fl::chrono::milliseconds(5)); // ok sleep for

        phase.store(2);
        sem.release();  // Signal "done"
    });

    sem.acquire();  // Wait for "started"
    FL_CHECK_EQ(phase.load(), 1);

    sem.acquire();  // Wait for "done"
    FL_CHECK_EQ(phase.load(), 2);

    t.join();
}

// ============================================================
// Level 2: task::coroutine high-level API
// ============================================================

FL_TEST_CASE("coroutine - task::coroutine runs function") {
    fl::atomic<bool> completed(false);
    fl::atomic<int> result(0);

    CoroutineConfig config;
    config.function = [&]() {
        result.store(42);
        completed.store(true);
    };
    config.name = "HighLevel";
    auto coro = task::coroutine(config);

    FL_CHECK(coro.isCoroutine());

    for (int elapsed = 0; elapsed < 500 && !completed.load(); elapsed += 5) {
        async_run(1000);
        delay(5);
    }

    FL_CHECK(completed.load());
    FL_CHECK_EQ(result.load(), 42);
}

FL_TEST_CASE("coroutine - two coroutines both complete") {
    fl::atomic<int> completed_count(0);

    CoroutineConfig config1;
    config1.function = [&]() {
        completed_count.fetch_add(1);
    };
    config1.name = "Coro1";
    auto coro1 = task::coroutine(config1);

    CoroutineConfig config2;
    config2.function = [&]() {
        completed_count.fetch_add(1);
    };
    config2.name = "Coro2";
    auto coro2 = task::coroutine(config2);

    for (int elapsed = 0; elapsed < 500 && completed_count.load() < 2; elapsed += 5) {
        async_run(1000);
        delay(5);
    }

    FL_CHECK_EQ(completed_count.load(), 2);
}

} // FL_TEST_FILE
