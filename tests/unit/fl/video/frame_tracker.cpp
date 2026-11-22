


// g++ --std=c++11 test.cpp


#include "fl/video/frame_tracker.h"
#include "fl/stl/stdint.h"
#include "test.h"

FL_TEST_FILE(FL_FILEPATH) {
using namespace fl;
using namespace fl::video;
FL_TEST_CASE("FrameTracker basic frame advancement") {
    FrameTracker tracker(1.0f);  // 1fps == 1000ms per frame
    uint32_t currentFrame, nextFrame;
    uint8_t amountOfNextFrame;

    //tracker.reset(1000);  // start at time 1000ms

    // Shift the time to 500 ms or 50% of the first frame
    tracker.get_interval_frames(500, &currentFrame, &nextFrame, &amountOfNextFrame);
    FL_CHECK(currentFrame == 0);
    FL_CHECK(nextFrame == 1);
    FL_CHECK(amountOfNextFrame == 127);
}

FL_TEST_CASE("FrameTracker timestamps and interpolation cross 32-bit microseconds") {
    FrameTracker tracker(1.0f);
    FL_CHECK_EQ(tracker.get_exact_timestamp_ms(4294), 4294000u);
    FL_CHECK_EQ(tracker.get_exact_timestamp_ms(4295), 4295000u);
    FL_CHECK_EQ(tracker.get_exact_timestamp_ms(4296), 4296000u);

    fl::u32 frame = 0;
    fl::u32 next = 0;
    fl::u8 progress = 0;
    tracker.get_interval_frames(4294500, &frame, &next, &progress);
    FL_CHECK_EQ(frame, 4294u);
    FL_CHECK_EQ(next, 4295u);
    FL_CHECK_EQ(progress, 127u);
    tracker.get_interval_frames(4295500, &frame, &next, &progress);
    FL_CHECK_EQ(frame, 4295u);
    FL_CHECK_EQ(next, 4296u);
    FL_CHECK_EQ(progress, 127u);
}

} // FL_TEST_FILE
