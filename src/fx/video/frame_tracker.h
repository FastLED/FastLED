#pragma once

#include "fl/stdint.h"

#include "fl/namespace.h"

#include "fl/int.h"
// #include <iostream>

// using namespace std;

namespace fl {

// Tracks the current frame number based on the time elapsed since the start of
// the animation.
class FrameTracker {
  public:
    FrameTracker(float fps);

    // Gets the current frame and the next frame number based on the current
    // time.
    void get_interval_frames(fl::u32 now, fl::u32 *frameNumber,
                             fl::u32 *nextFrameNumber,
                             uint8_t *amountOfNextFrame = nullptr) const;

    // Given a frame number, returns the exact timestamp in milliseconds that
    // the frame should be displayed.
    fl::u32 get_exact_timestamp_ms(fl::u32 frameNumber) const;

    fl::u32 microsecondsPerFrame() const { return mMicrosSecondsPerInterval; }

  private:
    fl::u32 mMicrosSecondsPerInterval;
    fl::u32 mStartTime = 0;
};

} // namespace fl
