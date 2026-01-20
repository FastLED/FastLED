

// g++ --std=c++11 test.cpp


#include "fl/fx/video/frame_tracker.h"
#include "fl/stl/stdint.h"
#include "doctest.h"
using namespace fl;
TEST_CASE("FrameTracker basic frame advancement") {
    FrameTracker tracker(1.0f);  // 1fps == 1000ms per frame
    uint32_t currentFrame, nextFrame;
    uint8_t amountOfNextFrame;
    
    //tracker.reset(1000);  // start at time 1000ms
    
    // Shift the time to 500 ms or 50% of the first frame
    tracker.get_interval_frames(500, &currentFrame, &nextFrame, &amountOfNextFrame);
    CHECK(currentFrame == 0);
    CHECK(nextFrame == 1);
    CHECK(amountOfNextFrame == 127);
}
