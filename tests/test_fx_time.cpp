
// g++ --std=c++11 test.cpp

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include "doctest.h"
#include "fx/time.h"

#include "namespace.h"
FASTLED_USING_NAMESPACE

TEST_CASE("TimeWarp basic functionality") {
    FASTLED_USING_NAMESPACE;

    SUBCASE("Initialization and normal time progression") {
        TimeScale tw(1000, 1.0f); // 1000 ms is the start time, speed is set at 1x
        CHECK(tw.time() == 1000);
        CHECK(tw.scale() == 1.0f);

        tw.update(2000);
        CHECK(tw.time() == 2000);
    }

    SUBCASE("Time scaling") {
        TimeScale tw(1000);
        tw.setScale(2.0f);  // now we are at 2x speed.
        CHECK(tw.time() == 1000);  // at t0 = 1000ms

        tw.update(1500);  // we give 500 at 2x => add 1000 to time counter.
        CHECK(tw.time() == 2000);

        tw.setScale(0.5f);  // Set half speed: 500ms.
        CHECK(tw.scale() == 0.5f);

        tw.update(2500);
        CHECK(tw.time() == 2500);
    }

    SUBCASE("Reset functionality") {
        TimeScale tw(1000, 1.0f);
        tw.update(2000);
        CHECK(tw.time() == 2000);

        tw.reset(3000);
        CHECK(tw.time() == 3000);

        tw.update(4000);
        CHECK(tw.time() == 4000);
    }

    SUBCASE("Wrap-around protection - prevent from going below start time") {
        TimeScale tw(1000, 1.0f);
        tw.update(1001);
        CHECK(tw.time() == 1001);
        tw.setScale(-1.0f);
        tw.update(2000);
        CHECK_EQ(tw.time(), 1000);
    }

}
