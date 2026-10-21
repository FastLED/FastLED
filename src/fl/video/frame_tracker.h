#pragma once

#include "fl/stl/int.h"
#include "fl/stl/noexcept.h"
// #include <iostream>

// using namespace std;

namespace fl {
namespace video {

// Tracks the current frame number based on the time elapsed since the start of
// the animation.
class FrameTracker {
  public:
    FrameTracker(float fps) FL_NO_EXCEPT;

    // Gets the current frame and the next frame number based on the current
    // time.
    void get_interval_frames(fl::u32 now, fl::u32 *frameNumber,
                             fl::u32 *nextFrameNumber,
                             u8 *amountOfNextFrame = nullptr) const FL_NO_EXCEPT;

    // Given a frame number, returns the exact timestamp in milliseconds that
    // the frame should be displayed.
    fl::u32 get_exact_timestamp_ms(fl::u32 frameNumber) const FL_NO_EXCEPT;

    fl::u32 microsecondsPerFrame() const FL_NO_EXCEPT { return mMicrosSecondsPerInterval; }

  private:
    fl::u32 mMicrosSecondsPerInterval;
    fl::u32 mStartTime = 0;
};

} // namespace video
using FrameTracker = video::FrameTracker;
} // namespace fl
