

// g++ --std=c++11 test.cpp



#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include "doctest.h"
#include "fx/video/frame_interpolator.h"
#include "fx/frame.h"
#include "namespace.h"



TEST_CASE("FrameInterpolator::selectFrames") {
    #if 1
    FrameInterpolator interpolator(2);  // Create an interpolator with capacity for 5 frames

    // Create some test frames with different timestamps
    Frame frame1(10, false);  // 10 pixels, no alpha
    Frame frame2(10, false);

    // Add frames with timestamps
    CHECK(interpolator.addWithTimestamp(frame1, 1000));
    CHECK(interpolator.addWithTimestamp(frame2, 2000));

    const Frame* selected1;
    const Frame* selected2;

    // Falls between two frames.
    bool selected = interpolator.selectFrames(1500, &selected1, &selected2);
    CHECK(selected);
    CHECK(selected1);
    CHECK(selected2);
    //CHECK(selected1 == &frame1);
    //CHECK(selected2 == &frame2);
    #endif
}




