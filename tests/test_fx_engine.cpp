
// g++ --std=c++11 test.cpp

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include "doctest.h"
#include "fx/fx_engine.h"

TEST_CASE("test_transition") {

    SUBCASE("Initial state") {
        Transition transition;
        CHECK(transition.getProgress(0) == 0);
        CHECK_FALSE(transition.isTransitioning(0));
    }

    SUBCASE("Start transition") {
        Transition transition;
        transition.start(100, 1000);
        CHECK(transition.isTransitioning(100));
        CHECK(transition.isTransitioning(1099));
        CHECK_FALSE(transition.isTransitioning(1100));
    }

    SUBCASE("Progress calculation") {
        Transition transition;
        transition.start(100, 1000);
        CHECK(transition.getProgress(100) == 0);
        CHECK(transition.getProgress(600) == 127);
        CHECK(transition.getProgress(1100) == 255);
    }

    SUBCASE("Progress before start time") {
        Transition transition;
        transition.start(100, 1000);
        CHECK(transition.getProgress(50) == 0);
    }

    SUBCASE("Progress after end time") {
        Transition transition;
        transition.start(100, 1000);
        CHECK(transition.getProgress(1200) == 255);
    }

    SUBCASE("Multiple transitions") {
        Transition transition;
        transition.start(100, 1000);
        CHECK(transition.isTransitioning(600));
        
        transition.start(2000, 500);
        CHECK_FALSE(transition.isTransitioning(1500));
        CHECK(transition.isTransitioning(2200));
        CHECK(transition.getProgress(2250) == 127);
    }

    SUBCASE("Zero duration transition") {
        Transition transition;
        transition.start(100, 0);
        CHECK_FALSE(transition.isTransitioning(100));
        CHECK(transition.getProgress(99) == 0);
        CHECK(transition.getProgress(100) == 255);
        CHECK(transition.getProgress(101) == 255);
    }
}

