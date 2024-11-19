#include <stdint.h>

#include "frame_tracker.h"
#include "namespace.h"

FASTLED_NAMESPACE_BEGIN


FrameTracker::FrameTracker(float fps) {
    mMicrosSecondsPerInterval = 1000000.0f / fps;
    mStartTime = 0;
    mPauseOffset = 0;
    mPauseTime = 0;
    mIsPaused = false;
}

void FrameTracker::reset(uint32_t startTime) {
    mStartTime = startTime;
    mPauseOffset = 0;
    mIsPaused = false;
}

void FrameTracker::pause(uint32_t now) {
    if (!mIsPaused) {
        mPauseTime = now;
        mIsPaused = true;
    }
}

void FrameTracker::resume(uint32_t now) {
    if (mIsPaused) {
        mPauseOffset += now - mPauseTime;
        mIsPaused = false;
    }
}

// Gets the current frame and the next frame number based on the current time.
void FrameTracker::get_interval_frames(uint32_t now, uint32_t* frameNumber, uint32_t* nextFrameNumber, uint8_t* amountOfNextFrame) const {
    // TODO: test out the mIsPaused condition.
    uint32_t adjustedNow = now - mPauseOffset;
    uint32_t elapsed = adjustedNow - mStartTime;
    uint32_t elapsedMicros = elapsed * 1000;
    uint32_t intervalNumber = elapsedMicros / mMicrosSecondsPerInterval;
    *frameNumber = intervalNumber;
    *nextFrameNumber = intervalNumber + 1;
    if (amountOfNextFrame) {
        // *amountOfNextFrame = (elapsedMicros % mMicrosSecondsPerInterval) / 1000;  // this isn't right, it won't say in the bounds of uint8_t
        *amountOfNextFrame = (elapsedMicros % mMicrosSecondsPerInterval) * 255 / mMicrosSecondsPerInterval / 1000;
        // cout << "elapsedMicros: " << elapsedMicros << ", mMicrosSecondsPerInterval: " << mMicrosSecondsPerInterval << ", *amountOfNextFrame: " << *amountOfNextFrame << endl;
    }
}

uint32_t FrameTracker::get_exact_timestamp_ms(uint32_t frameNumber) const {
    return mStartTime + (frameNumber * mMicrosSecondsPerInterval) / 1000;
}


FASTLED_NAMESPACE_END
