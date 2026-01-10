/// @file semaphore.cpp
/// @brief Tests for fl::counting_semaphore and fl::binary_semaphore

#include "fl/stl/semaphore.h"
#include "fl/stl/atomic.h"
#include "doctest.h"
#include "fl/stl/thread.h"
#include "fl/stl/new.h"
#include "fl/stl/type_traits.h"
#include "fl/stl/utility.h"
#include "ratio"
#include "thread_stub_stl.h"
#include "fl/stl/vector.h"


#if FASTLED_MULTITHREADED

TEST_CASE("fl::counting_semaphore basic operations") {
    SUBCASE("acquire and release single resource") {
        fl::counting_semaphore<5> sem(1);

        // Acquire the resource
        sem.acquire();

        // Try acquire should fail (count is 0)
        CHECK(sem.try_acquire() == false);

        // Release the resource
        sem.release();

        // Now try_acquire should succeed
        CHECK(sem.try_acquire() == true);
    }

    SUBCASE("multiple acquire and release") {
        fl::counting_semaphore<10> sem(3);

        // Acquire all 3 resources
        sem.acquire();
        sem.acquire();
        sem.acquire();

        // Try acquire should fail (count is 0)
        CHECK(sem.try_acquire() == false);

        // Release 2 resources
        sem.release(2);

        // Now we should be able to acquire 2
        CHECK(sem.try_acquire() == true);
        CHECK(sem.try_acquire() == true);
        CHECK(sem.try_acquire() == false);
    }

    SUBCASE("max() returns correct value") {
        fl::counting_semaphore<42> sem(0);
        CHECK(sem.max() == 42);
    }
}

TEST_CASE("fl::binary_semaphore basic operations") {
    SUBCASE("binary semaphore as simple flag") {
        fl::binary_semaphore sem(0);

        // Initially unavailable
        CHECK(sem.try_acquire() == false);

        // Signal
        sem.release();

        // Now available
        CHECK(sem.try_acquire() == true);

        // No longer available
        CHECK(sem.try_acquire() == false);
    }

    SUBCASE("binary semaphore max is 1") {
        fl::binary_semaphore sem(1);
        CHECK(sem.max() == 1);
    }
}

TEST_CASE("fl::counting_semaphore producer-consumer pattern") {
    constexpr int num_items = 10;
    fl::counting_semaphore<num_items> empty_slots(num_items);  // Initially all empty
    fl::counting_semaphore<num_items> filled_slots(0);         // Initially none filled
    fl::atomic<int> produced(0);
    fl::atomic<int> consumed(0);

    std::thread producer([&]() {  // okay std namespace
        for (int i = 0; i < num_items; ++i) {
            empty_slots.acquire();  // Wait for empty slot
            produced.fetch_add(1);
            filled_slots.release();  // Signal filled slot
        }
    });

    std::thread consumer([&]() {  // okay std namespace
        for (int i = 0; i < num_items; ++i) {
            filled_slots.acquire();  // Wait for filled slot
            consumed.fetch_add(1);
            empty_slots.release();   // Signal empty slot
        }
    });

    producer.join();
    consumer.join();

    CHECK(produced.load() == num_items);
    CHECK(consumed.load() == num_items);
}

TEST_CASE("fl::counting_semaphore multiple threads") {
    constexpr int num_threads = 5;
    constexpr int resources = 2;  // Only 2 can run concurrently

    fl::counting_semaphore<resources> sem(resources);
    fl::atomic<int> concurrent_count(0);
    fl::atomic<int> max_concurrent(0);
    fl::atomic<int> total_runs(0);

    fl::vector<std::thread> threads;  // okay std namespace
    for (int i = 0; i < num_threads; ++i) {
        threads.push_back(std::thread([&]() {  // okay std namespace
            sem.acquire();

            // Critical section - only 'resources' threads should be here
            int current = concurrent_count.fetch_add(1) + 1;

            // Update max concurrent if needed
            int expected_max = max_concurrent.load();
            while (current > expected_max &&
                   !max_concurrent.compare_exchange_weak(expected_max, current)) {
                expected_max = max_concurrent.load();
            }

            // Simulate work (reduced from 10ms to 5ms for performance)
            std::this_thread::sleep_for(std::chrono::milliseconds(5));  // okay std namespace

            total_runs.fetch_add(1);
            concurrent_count.fetch_sub(1);

            sem.release();
        }));
    }

    for (auto& t : threads) {
        t.join();
    }

    CHECK(total_runs.load() == num_threads);
    CHECK(max_concurrent.load() <= resources);
    CHECK(max_concurrent.load() > 0);
}

TEST_CASE("fl::counting_semaphore try_acquire_for") {
    fl::counting_semaphore<1> sem(0);  // Start with no resources

    SUBCASE("timeout when resource unavailable") {
        auto start = std::chrono::steady_clock::now();  // okay std namespace
        bool acquired = sem.try_acquire_for(std::chrono::milliseconds(20));  // okay std namespace (reduced from 50ms)
        auto elapsed = std::chrono::steady_clock::now() - start;  // okay std namespace

        CHECK(acquired == false);
        CHECK(elapsed >= std::chrono::milliseconds(15));  // Allow some tolerance  // okay std namespace (reduced from 40ms)
    }

    SUBCASE("immediate success when resource available") {
        sem.release();

        auto start = std::chrono::steady_clock::now();  // okay std namespace
        bool acquired = sem.try_acquire_for(std::chrono::milliseconds(100));  // okay std namespace
        auto elapsed = std::chrono::steady_clock::now() - start;  // okay std namespace

        CHECK(acquired == true);
        CHECK(elapsed < std::chrono::milliseconds(50));  // Should be much faster  // okay std namespace
    }
}

TEST_CASE("fl::binary_semaphore as thread synchronization") {
    fl::binary_semaphore ready(0);
    fl::binary_semaphore done(0);
    fl::atomic<int> shared_value(0);

    std::thread worker([&]() {  // okay std namespace
        // Wait for signal to start
        ready.acquire();

        // Do work
        shared_value.store(42);

        // Signal completion
        done.release();
    });

    // Signal worker to start (reduced from 10ms to 5ms for performance)
    std::this_thread::sleep_for(std::chrono::milliseconds(5));  // okay std namespace
    ready.release();

    // Wait for completion
    done.acquire();

    worker.join();

    CHECK(shared_value.load() == 42);
}

#else // !FASTLED_MULTITHREADED

TEST_CASE("fl::counting_semaphore single-threaded mode") {
    SUBCASE("basic acquire and release") {
        fl::counting_semaphore<5> sem(2);

        // Can acquire when count > 0
        sem.acquire();
        CHECK(sem.try_acquire() == true);

        // Cannot acquire when count = 0
        CHECK(sem.try_acquire() == false);

        // Release restores count
        sem.release();
        CHECK(sem.try_acquire() == true);
    }

    SUBCASE("release with update parameter") {
        fl::counting_semaphore<10> sem(0);

        sem.release(3);

        CHECK(sem.try_acquire() == true);
        CHECK(sem.try_acquire() == true);
        CHECK(sem.try_acquire() == true);
        CHECK(sem.try_acquire() == false);
    }

    SUBCASE("max() returns correct value") {
        fl::counting_semaphore<100> sem(0);
        CHECK(sem.max() == 100);
    }

    SUBCASE("try_acquire_for behaves like try_acquire") {
        fl::counting_semaphore<1> sem(1);

        CHECK(sem.try_acquire_for(std::chrono::milliseconds(100)) == true);  // okay std namespace
        CHECK(sem.try_acquire_for(std::chrono::milliseconds(100)) == false);  // okay std namespace
    }

    SUBCASE("try_acquire_until behaves like try_acquire") {
        fl::counting_semaphore<1> sem(1);
        auto future = std::chrono::steady_clock::now() + std::chrono::seconds(1);  // okay std namespace

        CHECK(sem.try_acquire_until(future) == true);
        CHECK(sem.try_acquire_until(future) == false);
    }
}

TEST_CASE("fl::binary_semaphore single-threaded mode") {
    SUBCASE("binary semaphore basic operations") {
        fl::binary_semaphore sem(0);

        CHECK(sem.try_acquire() == false);

        sem.release();
        CHECK(sem.try_acquire() == true);
        CHECK(sem.try_acquire() == false);
    }

    SUBCASE("binary semaphore max is 1") {
        fl::binary_semaphore sem(1);
        CHECK(sem.max() == 1);
    }
}

#endif // FASTLED_MULTITHREADED
