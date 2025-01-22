#pragma once

#include <stdint.h>

#include "fl/namespace.h"

// #include <iostream>

// using namespace std;

namespace fl {

// Tracks the current frame number based on the time elapsed since the start of the animation.
class FrameTracker {
  public:
    FrameTracker(float fps);
    
    // Gets the current frame and the next frame number based on the current time.
    void get_interval_frames(uint32_t now, uint32_t* frameNumber, uint32_t* nextFrameNumber, uint8_t* amountOfNextFrame = nullptr) const;

    // Given a frame number, returns the exact timestamp in milliseconds that the frame should be displayed.
    uint32_t get_exact_timestamp_ms(uint32_t frameNumber) const;

    
    uint32_t microsecondsPerFrame() const {
        return mMicrosSecondsPerInterval;
    }

  private:
    uint32_t mMicrosSecondsPerInterval;
    uint32_t mStartTime = 0;
};


}  // namespace fl
