
// g++ --std=c++11 test.cpp

#include "test.h"

#include "test.h"
#include "fx/time.h"

#include "fl/namespace.h"
FASTLED_USING_NAMESPACE

TEST_CASE("TimeWarp basic functionality") {
    FASTLED_USING_NAMESPACE;

    SUBCASE("Initialization and normal time progression") {
        TimeWarp tw(1000, 1.0f); // 1000 ms is the start time, speed is set at 1x
        CHECK(tw.time() == 0);
        CHECK(tw.scale() == 1.0f);

        tw.update(2000);
        CHECK(tw.time() == 1000);
    }

    SUBCASE("Time scaling") {
        TimeWarp tw(1000);
        tw.setSpeed(2.0f);  // now we are at 2x speed.
        CHECK(tw.time() == 0);  // at t0 = 1000ms

        tw.update(1500);  // we give 500 at 2x => add 1000 to time counter.
        CHECK(tw.time() == 1000);

        tw.setSpeed(0.5f);  // Set half speed: 500ms.
        CHECK(tw.scale() == 0.5f);

        tw.update(2500);
        CHECK(tw.time() == 1500);
    }

    SUBCASE("Reset functionality") {
        TimeWarp tw(1000, 1.0f);
        tw.update(2000);
        CHECK(tw.time() == 1000);

        tw.reset(3000);
        CHECK(tw.time() == 0);

        tw.update(4000);
        CHECK(tw.time() == 1000);
    }

    SUBCASE("Wrap-around protection - prevent from going below start time") {
        TimeWarp tw(1000, 1.0f);
        tw.update(1001);
        CHECK(tw.time() == 1);
        tw.setSpeed(-1.0f);
        tw.update(2000);
        CHECK_EQ(tw.time(), 0);
    }

}
