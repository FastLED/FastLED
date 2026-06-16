// Coroutine RPC bindings: testCoroutineBasic/Stop/Concurrent/Await/AwaitError/PromiseCallbacks/PromiseCatchCallback/ChainedAwait/All.
// Extracted from AutoResearchRemote.cpp as part of #3132 / meta #3127.

#include "fl/system/sketch_macros.h"
#if !defined(FASTLED_AUTORESEARCH_LOW_MEMORY) && !FL_PLATFORM_HAS_LARGE_MEMORY
#define FASTLED_AUTORESEARCH_LOW_MEMORY 1
#endif
#if !(defined(FASTLED_AUTORESEARCH_LOW_MEMORY) && FASTLED_AUTORESEARCH_LOW_MEMORY)

// Legacy debug macros (no-ops, kept for debugTest RPC function)
#define DEBUG_PRINT(x) do {} while(0)
#define DEBUG_PRINTLN(x) do {} while(0)

#include "AutoResearchRemote.h"
#include "AutoResearchBle.h"
#include "AutoResearchNet.h"
#include "AutoResearchOta.h"
#include "fl/remote/transport/serial.h"
#include "fl/system/heap.h"
#include "Common.h"
#include "AutoResearchTest.h"
#include "AutoResearchHelpers.h"
#include "fl/stl/sstream.h"
#include "fl/stl/unique_ptr.h"
#include "fl/stl/optional.h"
#include "fl/stl/json.h"
#include "fl/task/task.h"
#include "fl/task/executor.h"
#include "fl/math/wave/wave_perf_bench.h"
#include "fl/stl/atomic.h"
#include "fl/task/promise.h"
#include "fl/math/simd.h"
#include "AutoResearchSimd.h"
#include "AutoResearchAnimartrixBench.h"
#include "AutoResearchWave8Expand.h"
#include "AutoResearchParlioEncode.h"
#include "AutoResearchParlioStream.h"
#include "fl/chipsets/spi.h"
#include "fl/channels/config.h"
#include <Arduino.h>

#include "fl/net/ble.h"

#include "fl/codec/h264.h"
#include "fl/codec/mp4_parser.h"
#include "fl/stl/detail/memory_file_handle.h"
#include "fl/fx/frame.h"


void AutoResearchRemoteControl::bindCoroutineMethods() {
    // ========================================================================
    // Coroutine Tests - fl::task::coroutine() and fl::task::await()
    // ========================================================================

    // Test: Basic coroutine creation and completion
    mRemote->bind("testCoroutineBasic", [](const fl::json& args) -> fl::json {
        (void)args;
        fl::json r = fl::json::object();

        fl::atomic<bool> task_ran(false);
        fl::atomic<bool> task_completed(false);

        fl::task::CoroutineConfig cfg;
        cfg.func = [&task_ran, &task_completed]() {
            task_ran.store(true);
            delay(50);
            task_completed.store(true);
        };
        cfg.name = "test_basic";
        auto t = fl::task::coroutine(cfg);

        uint32_t start = millis();
        while (t.isRunning() && (millis() - start) < 2000) {
            delay(10);
        }

        bool passed = task_ran.load() && task_completed.load() && !t.isRunning();
        r.set("success", passed);
        r.set("taskRan", task_ran.load());
        r.set("taskCompleted", task_completed.load());
        r.set("isRunning", t.isRunning());
        r.set("durationMs", static_cast<int64_t>(millis() - start));
        return r;
    });

    // Test: Task stop() while running
    mRemote->bind("testCoroutineStop", [](const fl::json& args) -> fl::json {
        (void)args;
        fl::json r = fl::json::object();

        fl::atomic<bool> task_started(false);

        fl::task::CoroutineConfig cfg;
        cfg.func = [&task_started]() {
            task_started.store(true);
            while (true) {
                delay(10);
            }
        };
        cfg.name = "test_stop";
        auto t = fl::task::coroutine(cfg);

        uint32_t start = millis();
        while (!task_started.load() && (millis() - start) < 2000) {
            delay(10);
        }

        bool was_running = t.isRunning();
        t.stop();
        bool stopped = !t.isRunning();

        bool passed = task_started.load() && was_running && stopped;
        r.set("success", passed);
        r.set("taskStarted", task_started.load());
        r.set("wasRunning", was_running);
        r.set("stopped", stopped);
        r.set("durationMs", static_cast<int64_t>(millis() - start));
        return r;
    });

    // Test: Multiple concurrent coroutines
    mRemote->bind("testCoroutineConcurrent", [](const fl::json& args) -> fl::json {
        (void)args;
        fl::json r = fl::json::object();

        const int NUM_TASKS = 3;
        fl::atomic<int> completed_count(0);
        fl::atomic<bool> task_flags[NUM_TASKS];
        for (int i = 0; i < NUM_TASKS; i++) {
            task_flags[i].store(false);
        }

        fl::task::Handle tasks[NUM_TASKS];
        for (int i = 0; i < NUM_TASKS; i++) {
            fl::task::CoroutineConfig cfg;
            cfg.func = [i, &task_flags, &completed_count]() {
                delay(20 + i * 20);
                task_flags[i].store(true);
                completed_count.fetch_add(1);
            };
            cfg.name = "test_concurrent";
            tasks[i] = fl::task::coroutine(cfg);
        }

        uint32_t start = millis();
        while (completed_count.load() < NUM_TASKS && (millis() - start) < 3000) {
            delay(10);
        }

        bool all_completed = true;
        bool all_stopped = true;
        for (int i = 0; i < NUM_TASKS; i++) {
            if (!task_flags[i].load()) all_completed = false;
            if (tasks[i].isRunning()) all_stopped = false;
        }

        bool passed = all_completed && all_stopped && (completed_count.load() == NUM_TASKS);
        r.set("success", passed);
        r.set("completedCount", static_cast<int64_t>(completed_count.load()));
        r.set("allCompleted", all_completed);
        r.set("allStopped", all_stopped);
        r.set("durationMs", static_cast<int64_t>(millis() - start));
        return r;
    });

    // Test: Consumer coroutine awaits promise, producer coroutine fulfills it.
    // Verifies fl::task::await() truly blocks until the producer resolves the promise.
    // Main thread does NOT touch the promise — only the producer coroutine does.
    mRemote->bind("testCoroutineAwait", [](const fl::json& args) -> fl::json {
        (void)args;
        fl::json r = fl::json::object();

        auto promise_ptr = fl::make_shared<fl::task::Promise<int>>(fl::task::Promise<int>::create());
        fl::atomic<bool> consumer_started(false);
        fl::atomic<bool> consumer_finished(false);
        fl::atomic<int> consumer_value(0);
        fl::atomic<bool> consumer_ok(false);
        fl::atomic<bool> producer_started(false);
        fl::atomic<bool> producer_finished(false);

        // Consumer coroutine: starts first, calls fl::task::await() which should block
        // until the producer coroutine resolves the promise
        fl::task::CoroutineConfig consumer_cfg;
        consumer_cfg.func = [promise_ptr, &consumer_started, &consumer_finished,
                                 &consumer_value, &consumer_ok]() {
            consumer_started.store(true);
            // This should block the coroutine until producer fulfills the promise
            auto result = fl::task::await(*promise_ptr);
            if (result.ok()) {
                consumer_ok.store(true);
                consumer_value.store(result.value());
            }
            consumer_finished.store(true);
        };
        consumer_cfg.name = "await_consumer";
        auto consumer = fl::task::coroutine(consumer_cfg);

        // Small delay so consumer enters fl::task::await() before producer starts
        delay(50);

        // Producer coroutine: simulates async work, then resolves the promise
        fl::task::CoroutineConfig producer_cfg;
        producer_cfg.func = [promise_ptr, &producer_started, &producer_finished]() {
            producer_started.store(true);
            delay(200);  // simulate real async work (e.g. sensor read, network I/O)
            promise_ptr->complete_with_value(42);
            producer_finished.store(true);
        };
        producer_cfg.name = "await_producer";
        auto producer = fl::task::coroutine(producer_cfg);

        // Main thread waits for both coroutines to finish (polling only)
        uint32_t start = millis();
        while ((!consumer_finished.load() || producer.isRunning()) && (millis() - start) < 5000) {
            delay(10);
        }

        // Verify: consumer was truly blocked — it should not finish before producer
        bool passed = consumer_started.load() && consumer_finished.load()
                    && consumer_ok.load() && (consumer_value.load() == 42)
                    && producer_started.load() && producer_finished.load();
        r.set("success", passed);
        r.set("consumerStarted", consumer_started.load());
        r.set("consumerFinished", consumer_finished.load());
        r.set("consumerOk", consumer_ok.load());
        r.set("consumerValue", static_cast<int64_t>(consumer_value.load()));
        r.set("producerStarted", producer_started.load());
        r.set("producerFinished", producer_finished.load());
        r.set("durationMs", static_cast<int64_t>(millis() - start));
        return r;
    });

    // Test: Consumer coroutine awaits promise, producer coroutine rejects it.
    // Verifies that fl::task::await() properly propagates errors from producer.
    mRemote->bind("testCoroutineAwaitError", [](const fl::json& args) -> fl::json {
        (void)args;
        fl::json r = fl::json::object();

        auto promise_ptr = fl::make_shared<fl::task::Promise<int>>(fl::task::Promise<int>::create());
        fl::atomic<bool> consumer_finished(false);
        fl::atomic<bool> got_error(false);
        fl::atomic<bool> producer_finished(false);

        // Consumer coroutine: awaits promise, expects error
        fl::task::CoroutineConfig consumer_cfg;
        consumer_cfg.func = [promise_ptr, &consumer_finished, &got_error]() {
            auto result = fl::task::await(*promise_ptr);
            if (!result.ok()) {
                got_error.store(true);
            }
            consumer_finished.store(true);
        };
        consumer_cfg.name = "await_err_consumer";
        auto consumer = fl::task::coroutine(consumer_cfg);

        delay(50);  // let consumer enter await

        // Producer coroutine: rejects the promise with error
        fl::task::CoroutineConfig producer_cfg;
        producer_cfg.func = [promise_ptr, &producer_finished]() {
            delay(100);
            promise_ptr->complete_with_error(fl::task::Error("test error"));
            producer_finished.store(true);
        };
        producer_cfg.name = "await_err_producer";
        auto producer = fl::task::coroutine(producer_cfg);

        uint32_t start = millis();
        while ((!consumer_finished.load() || producer.isRunning()) && (millis() - start) < 5000) {
            delay(10);
        }

        bool passed = consumer_finished.load() && got_error.load() && producer_finished.load();
        r.set("success", passed);
        r.set("consumerFinished", consumer_finished.load());
        r.set("gotError", got_error.load());
        r.set("producerFinished", producer_finished.load());
        r.set("durationMs", static_cast<int64_t>(millis() - start));
        return r;
    });

    // Test: Promise then/catch_ callbacks fire correctly when fulfilled by a coroutine.
    // The promise is created with .then() and .catch_() callbacks attached BEFORE
    // the producer coroutine fulfills it, verifying callback dispatch works.
    mRemote->bind("testCoroutinePromiseCallbacks", [](const fl::json& args) -> fl::json {
        (void)args;
        fl::json r = fl::json::object();

        auto promise_ptr = fl::make_shared<fl::task::Promise<int>>(fl::task::Promise<int>::create());
        fl::atomic<bool> then_called(false);
        fl::atomic<int> then_value(0);
        fl::atomic<bool> catch_called(false);
        fl::atomic<bool> producer_finished(false);

        // Attach callbacks to the promise BEFORE producer runs
        promise_ptr->then([&then_called, &then_value](const int& val) {
            then_called.store(true);
            then_value.store(val);
        });
        promise_ptr->catch_([&catch_called](const fl::task::Error&) {
            catch_called.store(true);
        });

        // Producer coroutine fulfills the promise
        fl::task::CoroutineConfig producer_cfg;
        producer_cfg.func = [promise_ptr, &producer_finished]() {
            delay(100);
            promise_ptr->complete_with_value(99);
            producer_finished.store(true);
        };
        producer_cfg.name = "cb_producer";
        auto producer = fl::task::coroutine(producer_cfg);

        uint32_t start = millis();
        while (producer.isRunning() && (millis() - start) < 5000) {
            delay(10);
            promise_ptr->update();  // pump callbacks
        }
        promise_ptr->update();  // final pump

        // then() should fire, catch_() should NOT
        bool passed = then_called.load() && (then_value.load() == 99)
                    && !catch_called.load() && producer_finished.load();
        r.set("success", passed);
        r.set("thenCalled", then_called.load());
        r.set("thenValue", static_cast<int64_t>(then_value.load()));
        r.set("catchCalled", catch_called.load());
        r.set("producerFinished", producer_finished.load());
        r.set("durationMs", static_cast<int64_t>(millis() - start));
        return r;
    });

    // Test: Promise catch_ callback fires on rejection by a coroutine.
    mRemote->bind("testCoroutinePromiseCatchCallback", [](const fl::json& args) -> fl::json {
        (void)args;
        fl::json r = fl::json::object();

        auto promise_ptr = fl::make_shared<fl::task::Promise<int>>(fl::task::Promise<int>::create());
        fl::atomic<bool> then_called(false);
        fl::atomic<bool> catch_called(false);
        fl::atomic<bool> producer_finished(false);

        promise_ptr->then([&then_called](const int&) {
            then_called.store(true);
        });
        promise_ptr->catch_([&catch_called](const fl::task::Error&) {
            catch_called.store(true);
        });

        // Producer coroutine rejects the promise
        fl::task::CoroutineConfig producer_cfg;
        producer_cfg.func = [promise_ptr, &producer_finished]() {
            delay(100);
            promise_ptr->complete_with_error(fl::task::Error("rejection test"));
            producer_finished.store(true);
        };
        producer_cfg.name = "catch_producer";
        auto producer = fl::task::coroutine(producer_cfg);

        uint32_t start = millis();
        while (producer.isRunning() && (millis() - start) < 5000) {
            delay(10);
            promise_ptr->update();
        }
        promise_ptr->update();

        // catch_() should fire, then() should NOT
        bool passed = !then_called.load() && catch_called.load() && producer_finished.load();
        r.set("success", passed);
        r.set("thenCalled", then_called.load());
        r.set("catchCalled", catch_called.load());
        r.set("producerFinished", producer_finished.load());
        r.set("durationMs", static_cast<int64_t>(millis() - start));
        return r;
    });

    // Test: Pipeline of 3 coroutines passing data through promises.
    // coroutine A produces value -> promise1 -> coroutine B transforms -> promise2 -> coroutine C consumes
    mRemote->bind("testCoroutineChainedAwait", [](const fl::json& args) -> fl::json {
        (void)args;
        fl::json r = fl::json::object();

        auto p1 = fl::make_shared<fl::task::Promise<int>>(fl::task::Promise<int>::create());
        auto p2 = fl::make_shared<fl::task::Promise<int>>(fl::task::Promise<int>::create());
        fl::atomic<int> final_value(0);
        fl::atomic<bool> chain_complete(false);
        fl::atomic<bool> a_done(false);
        fl::atomic<bool> b_done(false);
        fl::atomic<bool> c_done(false);

        // Coroutine C: end of chain, awaits p2
        fl::task::CoroutineConfig cfg_c;
        cfg_c.func = [p2, &final_value, &chain_complete, &c_done]() {
            auto result = fl::task::await(*p2);
            if (result.ok()) {
                final_value.store(result.value());
            }
            c_done.store(true);
            chain_complete.store(true);
        };
        cfg_c.name = "chain_c";
        auto tc = fl::task::coroutine(cfg_c);

        // Coroutine B: middle of chain, awaits p1, transforms, fulfills p2
        fl::task::CoroutineConfig cfg_b;
        cfg_b.func = [p1, p2, &b_done]() {
            auto result = fl::task::await(*p1);
            if (result.ok()) {
                // Transform: multiply by 10
                p2->complete_with_value(result.value() * 10);
            } else {
                p2->complete_with_error(result.error());
            }
            b_done.store(true);
        };
        cfg_b.name = "chain_b";
        auto tb = fl::task::coroutine(cfg_b);

        // Coroutine A: head of chain, produces the initial value into p1
        fl::task::CoroutineConfig cfg_a;
        cfg_a.func = [p1, &a_done]() {
            delay(100);  // simulate work
            p1->complete_with_value(7);
            a_done.store(true);
        };
        cfg_a.name = "chain_a";
        auto ta = fl::task::coroutine(cfg_a);

        uint32_t start = millis();
        while (!chain_complete.load() && (millis() - start) < 5000) {
            delay(10);
        }

        // 7 * 10 = 70
        bool passed = chain_complete.load() && (final_value.load() == 70)
                    && a_done.load() && b_done.load() && c_done.load();
        r.set("success", passed);
        r.set("finalValue", static_cast<int64_t>(final_value.load()));
        r.set("aDone", a_done.load());
        r.set("bDone", b_done.load());
        r.set("cDone", c_done.load());
        r.set("durationMs", static_cast<int64_t>(millis() - start));
        return r;
    });

    // Run all coroutine tests sequentially
    mRemote->bind("testCoroutineAll", [this](const fl::json& args) -> fl::json {
        (void)args;
        fl::json r = fl::json::object();

        const char* test_names[] = {
            "testCoroutineBasic",
            "testCoroutineStop",
            "testCoroutineConcurrent",
            "testCoroutineAwait",
            "testCoroutineAwaitError",
            "testCoroutinePromiseCallbacks",
            "testCoroutinePromiseCatchCallback",
            "testCoroutineChainedAwait"
        };
        const int num_tests = 8;

        int passed = 0;
        int failed = 0;
        fl::json results = fl::json::object();

        for (int i = 0; i < num_tests; i++) {
            fl::json empty_args = fl::json::object();
            auto bound = mRemote->get<fl::json(const fl::json&)>(test_names[i]);
            if (bound.ok()) {
                fl::json test_result = bound.value()(empty_args);
                bool success = false;
                if (test_result.contains("success")) {
                    auto val = test_result["success"].as_bool();
                    success = val.has_value() && val.value();
                }
                if (success) {
                    passed++;
                    FL_PRINT("[COROUTINE] PASS: " << test_names[i]);
                } else {
                    failed++;
                    FL_PRINT("[COROUTINE] FAIL: " << test_names[i]);
                }
                results.set(test_names[i], test_result);
            } else {
                failed++;
                fl::json err = fl::json::object();
                err.set("success", false);
                err.set("error", "Method not found");
                results.set(test_names[i], err);
                FL_PRINT("[COROUTINE] FAIL: " << test_names[i] << " (not found)");
            }
        }

        r.set("success", failed == 0);
        r.set("passed", static_cast<int64_t>(passed));
        r.set("failed", static_cast<int64_t>(failed));
        r.set("total", static_cast<int64_t>(num_tests));
        r.set("results", results);
        return r;
    });
}

#endif // !(FASTLED_AUTORESEARCH_LOW_MEMORY)
