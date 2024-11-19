

// g++ --std=c++11 test.cpp

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include "doctest.h"
#include "fx/frame.h"
#include "fx/video/frame_tracker.h"
#include "namespace.h"

#include "namespace.h"
FASTLED_USING_NAMESPACE

TEST_CASE("FrameTracker basic initialization") {
    FrameTracker tracker(30.0f);
    uint32_t currentFrame, nextFrame;
    uint8_t amountOfNextFrame;
    tracker.reset(1000); // start at time 1000ms
    tracker.get_interval_frames(1000, &currentFrame, &nextFrame, &amountOfNextFrame);
    CHECK(currentFrame == 0);
    CHECK(nextFrame == 1);
    CHECK(amountOfNextFrame == 0);
}
