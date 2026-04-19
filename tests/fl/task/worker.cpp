#include "test.h"
#include "fl/task/worker.h"
#include "fl/stl/atomic.h"

FL_TEST_FILE(FL_FILEPATH) {

FL_TEST_CASE("Worker: synchronous fallback runs fn on the calling thread") {
    // On host / non-ESP32 / single-core builds the worker falls through to
    // a synchronous call. This is the contract we rely on to keep sketches
    // portable across platforms with and without cross-core affinity.
    fl::task::Worker worker(0);
    fl::atomic<int> counter(0);

    worker.run([&counter]() { counter.fetch_add(1); });

    FL_CHECK_EQ(1, counter.load());
}

FL_TEST_CASE("Worker: run() is re-entrant across multiple posts") {
    fl::task::Worker worker(0);
    fl::atomic<int> counter(0);

    for (int i = 0; i < 5; ++i) {
        worker.run([&counter]() { counter.fetch_add(1); });
    }

    FL_CHECK_EQ(5, counter.load());
}

FL_TEST_CASE("Worker: run() with a null functor is a no-op") {
    // Construct a default-initialised (empty) function and pass it to
    // run(). The worker must not crash or block; behaviour is
    // "do nothing" on the happy path.
    fl::task::Worker worker(0);
    fl::function<void()> empty;
    worker.run(empty);  // no assertion — we're verifying it doesn't crash
    FL_CHECK(true);
}

FL_TEST_CASE("Worker: isPinned() is false on host / non-ESP32 builds") {
    // Host tests build without FL_HAS_MULTICORE_AFFINITY so the worker
    // falls through to the synchronous path and does not spawn a pinned
    // FreeRTOS task. This encodes that contract.
    fl::task::Worker worker(0);
    FL_CHECK_FALSE(worker.isPinned());
}

FL_TEST_CASE("Worker: coreId() round-trips the requested core") {
    fl::task::Worker worker_default;
    FL_CHECK_EQ(-1, worker_default.coreId());

    fl::task::Worker worker_zero(0);
    FL_CHECK_EQ(0, worker_zero.coreId());

    fl::task::Worker worker_one(1);
    FL_CHECK_EQ(1, worker_one.coreId());
}

} // FL_TEST_FILE
