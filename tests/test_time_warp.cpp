
// g++ --std=c++11 test.cpp

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include "doctest.h"
#include "fx/detail/time_warp.h"

#include "namespace.h"
FASTLED_USING_NAMESPACE

TEST_CASE("TimeWarp basic functionality") {
    FASTLED_USING_NAMESPACE;

    SUBCASE("Initialization and normal time progression") {
        TimeWarp tw(1000, 1.0f);
        CHECK(tw.getTime() == 1000);
        CHECK(tw.getTimeScale() == 1.0f);

        tw.update(2000);
        CHECK(tw.getTime() == 2000);
    }

    SUBCASE("Time scaling") {
        TimeWarp tw(1000, 2.0f);
        CHECK(tw.getTimeScale() == 2.0f);

        tw.update(1500);
        CHECK(tw.getTime() == 2000);

        tw.setTimeScale(0.5f);
        CHECK(tw.getTimeScale() == 0.5f);

        tw.update(2500);
        CHECK(tw.getTime() == 2500);
    }

    SUBCASE("Reset functionality") {
        TimeWarp tw(1000, 1.0f);
        tw.update(2000);
        CHECK(tw.getTime() == 2000);

        uint32_t resetTime = tw.reset(3000);
        CHECK(resetTime == 3000);
        CHECK(tw.getTime() == 3000);

        tw.update(4000);
        CHECK(tw.getTime() == 4000);
    }

    SUBCASE("Wrap-around protection - prevent from going below start time") {
        TimeWarp tw(1000, 1.0f);
        tw.update(1001);
        CHECK(tw.getTime() == 1001);
        tw.setTimeScale(-1.0f);
        tw.update(2000);
        CHECK_EQ(tw.getTime(), 1000);
    }

}
