// ok cpp include
/// @file coroutine.cpp
/// @brief Unit tests for stub platform coroutine runner (independent of async)
///
/// Tests the coroutine system at multiple levels:
/// 1. Binary semaphore (lowest level primitive)
/// 2. CoroutineContext + CoroutineRunner (mid-level coordination)
/// 3. task::coroutine (high-level API)

#include "test.h"
#include "fl/stl/task.h"
#include "fl/stl/atomic.h"
#include "fl/stl/thread.h"
#include "fl/stl/mutex.h"
#include "fl/stl/chrono.h"
#include "fl/stl/function.h"
#include "fl/stl/string.h"
#include "fl/stl/semaphore.h"
#include "fl/delay.h"
#include "fl/stl/async.h"
#include "platforms/stub/task_coroutine_stub.h"
#include "platforms/stub/coroutine_runner.h"
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
// Level 2: CoroutineContext tests
// ============================================================

FL_TEST_CASE("coroutine - context creation and flags") {
    auto ctx = fl::detail::CoroutineContext::create();
    FL_CHECK(ctx != nullptr);
    FL_CHECK_FALSE(ctx->is_completed());
    FL_CHECK_FALSE(ctx->should_stop());
    FL_CHECK_FALSE(ctx->is_thread_ready());

    ctx->set_completed(true);
    FL_CHECK(ctx->is_completed());
    ctx->set_should_stop(true);
    FL_CHECK(ctx->should_stop());
    ctx->set_thread_ready(true);
    FL_CHECK(ctx->is_thread_ready());
}

FL_TEST_CASE("coroutine - context wakeup semaphore wakes thread") {
    auto ctx = fl::detail::CoroutineContext::create();
    fl::atomic<bool> thread_woke(false);

    fl::thread t([&]() {
        ctx->set_thread_ready(true);
        ctx->wait();  // Blocks on wakeup semaphore
        thread_woke.store(true);
    });

    while (!ctx->is_thread_ready()) {
        fl::this_thread::sleep_for(fl::chrono::milliseconds(1)); // ok sleep for
    }

    ctx->wakeup_semaphore().release();
    t.join();
    FL_CHECK(thread_woke.load());
}

// ============================================================
// Level 3: CoroutineRunner + manual thread (two-phase protocol)
// ============================================================

FL_TEST_CASE("coroutine - runner enqueue and run") {
    auto& runner = fl::detail::CoroutineRunner::instance();
    auto& main_sema = runner.get_main_thread_semaphore();

    auto ctx = fl::detail::CoroutineContext::create();
    fl::atomic<bool> func_executed(false);

    // Simulate a coroutine thread using the two-phase protocol
    fl::thread t([&]() {
        ctx->set_thread_ready(true);
        ctx->wait();  // Block until runner wakes us

        if (ctx->should_stop()) {
            ctx->set_completed(true);
            return;
        }

        // Phase 1: signal "started"
        main_sema.release();

        // Execute user function
        func_executed.store(true);

        // Phase 2: signal "done"
        ctx->set_completed(true);
        main_sema.release();
    });

    while (!ctx->is_thread_ready()) {
        fl::this_thread::sleep_for(fl::chrono::milliseconds(1)); // ok sleep for
    }

    runner.enqueue(ctx);
    runner.run(1000);

    t.join();
    FL_CHECK(func_executed.load());
    FL_CHECK(ctx->is_completed());

    runner.remove(ctx);
}

// ============================================================
// Level 4: task::coroutine high-level API
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
        async_run(1);
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
        async_run(1);
        delay(5);
    }

    FL_CHECK_EQ(completed_count.load(), 2);
}

} // FL_TEST_FILE
