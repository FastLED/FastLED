/// @file test_delay_async.cpp
/// Unit tests for fl::delay() with async task pumping

#include "fl/delay.h"
#include "fl/async.h"
#include "test.h"

// Test async runner that tracks update calls
class TestAsyncRunner : public fl::async_runner {
public:
    int update_count = 0;

    void update() override {
        update_count++;
    }

    bool has_active_tasks() const override {
        return false;
    }

    size_t active_task_count() const override {
        return 0;
    }

    void reset() {
        update_count = 0;
    }
};

FL_TEST_CASE("fl::delay(0) returns immediately") {
    fl::delay(0);
    FL_CHECK(true);  // Should not hang
}

FL_TEST_CASE("fl::delay(0, false) returns immediately") {
    fl::delay(0, false);
    FL_CHECK(true);  // Should not hang
}

FL_TEST_CASE("fl::delay(0, true) returns immediately") {
    fl::delay(0, true);
    FL_CHECK(true);  // Should not hang
}

FL_TEST_CASE("fl::delay(ms, false) does not pump async tasks") {
    TestAsyncRunner runner;
    fl::AsyncManager::instance().register_runner(&runner);

    runner.reset();
    fl::delay(10, false);  // Simple delay without async pumping

    // Async runner should NOT have been called
    FL_CHECK(runner.update_count == 0);

    fl::AsyncManager::instance().unregister_runner(&runner);
}

FL_TEST_CASE("fl::delay(ms, true) pumps async tasks") {
    TestAsyncRunner runner;
    fl::AsyncManager::instance().register_runner(&runner);

    runner.reset();
    fl::delay(10, true);  // Delay with async pumping (default)

    // Async runner should have been called multiple times
    FL_CHECK(runner.update_count > 0);

    fl::AsyncManager::instance().unregister_runner(&runner);
}

FL_TEST_CASE("fl::delay(ms) defaults to async pumping") {
    TestAsyncRunner runner;
    fl::AsyncManager::instance().register_runner(&runner);

    runner.reset();
    fl::delay(10);  // Default should be run_async=true

    // Async runner should have been called
    FL_CHECK(runner.update_count > 0);

    fl::AsyncManager::instance().unregister_runner(&runner);
}

FL_TEST_CASE("fl::delayMs(ms) delegates to fl::delay with async") {
    TestAsyncRunner runner;
    fl::AsyncManager::instance().register_runner(&runner);

    runner.reset();
    fl::delayMs(10);  // Should delegate to delay(10, true)

    // Async runner should have been called
    FL_CHECK(runner.update_count > 0);

    fl::AsyncManager::instance().unregister_runner(&runner);
}

FL_TEST_CASE("fl::delayMs(ms, false) disables async pumping") {
    TestAsyncRunner runner;
    fl::AsyncManager::instance().register_runner(&runner);

    runner.reset();
    fl::delayMs(10, false);  // Explicit disable

    // Async runner should NOT have been called
    FL_CHECK(runner.update_count == 0);

    fl::AsyncManager::instance().unregister_runner(&runner);
}

FL_TEST_CASE("fl::delayMillis() does not pump async (legacy)") {
    TestAsyncRunner runner;
    fl::AsyncManager::instance().register_runner(&runner);

    runner.reset();
    fl::delayMillis(10);  // Legacy function should not pump async

    // Async runner should NOT have been called (backward compatibility)
    FL_CHECK(runner.update_count == 0);

    fl::AsyncManager::instance().unregister_runner(&runner);
}
