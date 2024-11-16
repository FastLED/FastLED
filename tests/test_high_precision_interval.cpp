
// g++ --std=c++11 test.cpp

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include "doctest.h"
#include "fx/video/high_precision_interval.h"
#include "namespace.h"

FASTLED_USING_NAMESPACE


TEST_CASE("HighPrecisionInterval basic functionality") {
    // 1000000 microseconds = 1 second per interval
    HighPrecisionInterval interval(1000000); 
    uint32_t timestamp;

    SUBCASE("Initial state") {
        interval.reset(0);
        CHECK(interval.isPaused() == false);
        CHECK(interval.needsFrame(0, &timestamp) == false);
    }

    SUBCASE("Frame timing") {
        interval.reset(0);
        // At t=500ms, shouldn't need frame yet
        CHECK(interval.needsFrame(500, &timestamp) == false);
        // At t=1000ms (1 second), should need frame
        CHECK(interval.needsFrame(1000, &timestamp) == true);
        CHECK(timestamp == 1000);
        interval.incrementIntervalCounter();
        // After increment, shouldn't need frame yet
        CHECK(interval.needsFrame(1000, &timestamp) == false);
    }

    SUBCASE("Pause and resume") {
        interval.reset(0);
        // Pause at t=500ms
        interval.pause(500);
        CHECK(interval.isPaused() == true);
        CHECK(interval.needsFrame(1000, &timestamp) == false);
        // Resume at t=1000ms
        interval.resume(1000);
        CHECK(interval.isPaused() == false);
        // Should still need 500ms more
        CHECK(interval.needsFrame(1000, &timestamp) == false);
        CHECK(interval.needsFrame(1500, &timestamp) == true);
        CHECK(timestamp == 1500);
    }

    SUBCASE("Multiple intervals") {
        interval.reset(0);
        // First interval
        CHECK(interval.needsFrame(1000, &timestamp) == true);
        CHECK(timestamp == 1000);
        interval.incrementIntervalCounter();
        // Second interval
        CHECK(interval.needsFrame(2000, &timestamp) == true);
        CHECK(timestamp == 2000);
        interval.incrementIntervalCounter();
        // Third interval
        CHECK(interval.needsFrame(3000, &timestamp) == true);
        CHECK(timestamp == 3000);
    }
}


