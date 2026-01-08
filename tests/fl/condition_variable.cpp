/// @file condition_variable.cpp
/// @brief Tests for fl::condition_variable wrapper

#include "fl/stl/condition_variable.h"
#include "fl/stl/mutex.h"
#include "fl/stl/atomic.h"
#include "doctest.h"
#include "__chrono/duration.h"
#include "__new/placement_new_delete.h"
#include "__thread/this_thread.h"
#include "__thread/thread.h"
#include "__type_traits/invoke.h"
#include "__utility/move.h"
#include "fl/stl/allocator.h"
#include "fl/stl/vector.h"
#include "mutex_stub_stl.h"
#include "thread_stub_stl.h"

#if FASTLED_MULTITHREADED


TEST_CASE("fl::condition_variable basic operations") {
    fl::mutex mtx;
    fl::condition_variable cv;
    fl::atomic<bool> ready(false);
    fl::atomic<bool> processed(false);

    SUBCASE("notify_one wakes waiting thread") {
        std::thread worker([&]() {  // okay std namespace - fl::thread not available
            fl::unique_lock<fl::mutex> lock(mtx);
            ready.store(true);
            cv.notify_one();  // Signal main thread

            // Wait for main thread to process
            cv.wait(lock, [&]() { return processed.load(); });
        });

        // Wait for worker to be ready
        fl::unique_lock<fl::mutex> lock(mtx);
        cv.wait(lock, [&]() { return ready.load(); });

        // Signal we've processed
        processed.store(true);
        cv.notify_one();

        lock.unlock();
        worker.join();

        CHECK(ready.load() == true);
        CHECK(processed.load() == true);
    }

    SUBCASE("notify_all wakes multiple threads") {
        constexpr int num_threads = 3;
        fl::atomic<int> wake_count(0);
        fl::vector<std::thread> threads;  // okay std namespace - fl::thread not available

        for (int i = 0; i < num_threads; ++i) {
            threads.push_back(std::thread([&]() {  // okay std namespace
                fl::unique_lock<fl::mutex> lock(mtx);
                cv.wait(lock, [&]() { return ready.load(); });
                wake_count.fetch_add(1);
            }));
        }

        // Give threads time to start waiting
        std::this_thread::sleep_for(std::chrono::milliseconds(10));  // okay std namespace

        // Wake all threads
        {
            fl::unique_lock<fl::mutex> lock(mtx);
            ready.store(true);
        }
        cv.notify_all();

        // Wait for all threads to finish
        for (auto& t : threads) {
            t.join();
        }

        CHECK(wake_count.load() == num_threads);
    }
}

TEST_CASE("fl::condition_variable with predicate") {
    fl::mutex mtx;
    fl::condition_variable cv;
    fl::atomic<int> value(0);

    std::thread producer([&]() {  // okay std namespace - fl::thread not available
        for (int i = 1; i <= 5; ++i) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));  // okay std namespace
            {
                fl::unique_lock<fl::mutex> lock(mtx);
                value.store(i);
            }
            cv.notify_one();
        }
    });

    std::thread consumer([&]() {  // okay std namespace - fl::thread not available
        fl::unique_lock<fl::mutex> lock(mtx);
        cv.wait(lock, [&]() { return value.load() >= 5; });
        CHECK(value.load() == 5);
    });

    producer.join();
    consumer.join();
}

#else // !FASTLED_MULTITHREADED

TEST_CASE("fl::condition_variable single-threaded mode") {
    fl::mutex mtx;
    fl::condition_variable cv;
    (void)mtx;  // Suppress unused variable warning

    SUBCASE("notify operations are no-ops") {
        // These should compile and run without error in single-threaded mode
        cv.notify_one();
        cv.notify_all();
        CHECK(true);
    }

    // Note: Cannot test wait() in single-threaded mode as it would trigger
    // an assertion failure by design (waiting in single-threaded would deadlock)
}

#endif // FASTLED_MULTITHREADED
