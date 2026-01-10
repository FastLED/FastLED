#include "fl/stl/thread.h"
#include "fl/stl/mutex.h"
#include "fl/stl/atomic.h"
#include "fl/stl/algorithm.h"
#include "fl/stl/ostream.h"
#include "fl/stl/thread.h"
#include "fl/stl/type_traits.h"
#include "fl/stl/utility.h"
#include "doctest.h"
#include "fl/stl/move.h"
#include "ios"
#include "mutex_stub_stl.h"

TEST_CASE("fl::thread - basic construction and joinable") {
    // Default constructed thread is not joinable
    fl::thread t1;
    REQUIRE(!t1.joinable());

    // Thread with function is joinable
    bool executed = false;
    fl::thread t2([&executed]() {
        executed = true;
    });
    REQUIRE(t2.joinable());
    t2.join();
    REQUIRE(executed);
    REQUIRE(!t2.joinable()); // After join, not joinable
}

TEST_CASE("fl::thread - this_thread::get_id") {
    auto main_id = fl::this_thread::get_id();

    fl::thread::id thread_id;
    fl::thread t([&thread_id]() {
        thread_id = fl::this_thread::get_id();
    });
    t.join();

    // Thread ID should be different from main thread
    REQUIRE(thread_id != main_id);
}

TEST_CASE("fl::thread - thread with arguments") {
    int result = 0;
    fl::mutex m;

    auto thread_func = [&](int a, int b) {
        fl::unique_lock<fl::mutex> lock(m);
        result = a + b;
    };

    fl::thread t(thread_func, 10, 20);
    t.join();

    REQUIRE(result == 30);
}

TEST_CASE("fl::thread - move semantics") {
    fl::atomic<bool> executed(false);

    fl::thread t1([&executed]() {
        executed.store(true);
    });

    REQUIRE(t1.joinable());

    // Move construct
    fl::thread t2(fl::move(t1));
    REQUIRE(!t1.joinable());
    REQUIRE(t2.joinable());

    t2.join();
    REQUIRE(executed.load());
}

TEST_CASE("fl::thread - detach") {
    fl::atomic<bool> started(false);

    fl::thread t([&started]() {
        started.store(true);
        // Thread does some work
        for (volatile int i = 0; i < 1000000; ++i) {}
    });

    REQUIRE(t.joinable());
    t.detach();
    REQUIRE(!t.joinable());

    // Wait for thread to start
    while (!started.load()) {
        fl::this_thread::yield();
    }
}

TEST_CASE("fl::thread - hardware_concurrency") {
    unsigned int cores = fl::thread::hardware_concurrency();
    REQUIRE(cores >= 1);
}

TEST_CASE("fl::thread - yield") {
    // Just verify it compiles and runs without crashing
    fl::this_thread::yield();
}

TEST_CASE("fl::thread - multiple threads") {
    constexpr int num_threads = 4;
    constexpr int iterations = 1000;

    fl::atomic<int> counter(0);
    fl::thread threads[num_threads];

    for (int i = 0; i < num_threads; ++i) {
        threads[i] = fl::thread([&counter]() {
            for (int j = 0; j < iterations; ++j) {
                counter.fetch_add(1);
            }
        });
    }

    for (int i = 0; i < num_threads; ++i) {
        threads[i].join();
    }

    REQUIRE(counter.load() == num_threads * iterations);
}

TEST_CASE("fl::thread - thread with mutex synchronization") {
    fl::mutex m;
    int shared_value = 0;
    constexpr int num_increments = 1000;

    fl::thread t1([&]() {
        for (int i = 0; i < num_increments; ++i) {
            fl::unique_lock<fl::mutex> lock(m);
            ++shared_value;
        }
    });

    fl::thread t2([&]() {
        for (int i = 0; i < num_increments; ++i) {
            fl::unique_lock<fl::mutex> lock(m);
            ++shared_value;
        }
    });

    t1.join();
    t2.join();

    REQUIRE(shared_value == 2 * num_increments);
}
