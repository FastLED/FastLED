/// @file delay.cpp
/// Unit tests for fl/system/delay.h: nanosecond delays, runtime delays, and async pumping

#include "fl/system/delay.h"
#include "fl/stl/async.h"
#include "test.h"

FL_TEST_FILE(FL_FILEPATH) {

// ============================================================================
// Test Suite: Compile-time Template Delays
// ============================================================================

FL_TEST_CASE("delayNanoseconds<50>() compiles") {
  fl::delayNanoseconds<50>();
  FL_CHECK(true);
}

FL_TEST_CASE("delayNanoseconds<100>() compiles") {
  fl::delayNanoseconds<100>();
  FL_CHECK(true);
}

FL_TEST_CASE("delayNanoseconds<350>() compiles (WS2812 T1)") {
  fl::delayNanoseconds<350>();
  FL_CHECK(true);
}

FL_TEST_CASE("delayNanoseconds<700>() compiles (WS2812 T2)") {
  fl::delayNanoseconds<700>();
  FL_CHECK(true);
}

FL_TEST_CASE("delayNanoseconds<1000>() compiles (1 microsecond)") {
  fl::delayNanoseconds<1000>();
  FL_CHECK(true);
}

FL_TEST_CASE("delayNanoseconds<10000>() compiles (10 microseconds)") {
  fl::delayNanoseconds<10000>();
  FL_CHECK(true);
}

// ============================================================================
// Test Suite: Runtime Delays
// ============================================================================

FL_TEST_CASE("delayNanoseconds(0) does nothing") {
  fl::delayNanoseconds(0);
  FL_CHECK(true);
}

FL_TEST_CASE("delayNanoseconds(50) compiles and runs") {
  fl::delayNanoseconds(50);
  FL_CHECK(true);
}

FL_TEST_CASE("delayNanoseconds(350) compiles and runs (WS2812 T1)") {
  fl::delayNanoseconds(350);
  FL_CHECK(true);
}

FL_TEST_CASE("delayNanoseconds(700) compiles and runs (WS2812 T2)") {
  fl::delayNanoseconds(700);
  FL_CHECK(true);
}

FL_TEST_CASE("delayNanoseconds(1000) compiles and runs (1 microsecond)") {
  fl::delayNanoseconds(1000);
  FL_CHECK(true);
}

FL_TEST_CASE("delayNanoseconds with explicit clock frequency - 16 MHz") {
  fl::delayNanoseconds(100, 16000000);
  FL_CHECK(true);
}

FL_TEST_CASE("delayNanoseconds with explicit clock frequency - 80 MHz (ESP32)") {
  fl::delayNanoseconds(350, 80000000);
  FL_CHECK(true);
}

FL_TEST_CASE("delayNanoseconds with explicit clock frequency - 160 MHz (ESP32 turbo)") {
  fl::delayNanoseconds(700, 160000000);
  FL_CHECK(true);
}

// ============================================================================
// Test Suite: LED Protocol Delays
// ============================================================================

FL_TEST_CASE("WS2812 LED Protocol Delays") {
  fl::delayNanoseconds<350>();
  fl::delayNanoseconds<800>();
  fl::delayNanoseconds<700>();
  fl::delayNanoseconds<600>();
  FL_CHECK(true);
}

FL_TEST_CASE("SK6812 LED Protocol Delays") {
  fl::delayNanoseconds<300>();
  fl::delayNanoseconds<600>();
  fl::delayNanoseconds<300>();
  FL_CHECK(true);
}

FL_TEST_CASE("Various nanosecond delays") {
  fl::delayNanoseconds<50>();
  fl::delayNanoseconds<100>();
  fl::delayNanoseconds<500>();
  fl::delayNanoseconds<1000>();
  fl::delayNanoseconds<5000>();
  fl::delayNanoseconds<10000>();
  FL_CHECK(true);
}

FL_TEST_CASE("Mix of compile-time and runtime delays") {
  fl::delayNanoseconds<350>();
  fl::delayNanoseconds(700);
  fl::delayNanoseconds(600, 80000000);
  fl::delayNanoseconds<300>();
  FL_CHECK(true);
}

// ============================================================================
// Test Suite: Async Delay Pumping
// ============================================================================

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

} // FL_TEST_FILE
