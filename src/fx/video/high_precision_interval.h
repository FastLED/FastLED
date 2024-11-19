#pragma once

#include "namespace.h"

// #include <iostream>

using namespace std;

FASTLED_NAMESPACE_BEGIN

// Tracks the current frame number based on the time elapsed since the start of the animation.
class FrameTracker {
  public:
    FrameTracker(float fps) {
        mMicrosSecondsPerInterval = 1000000.0f / fps;
        mIntervalCounter = 0;
        mStartTime = 0;
        mPauseOffset = 0;
        mPauseTime = 0;
        mIsPaused = false;
    }

    void reset(uint32_t startTime) {
        mStartTime = startTime;
        mIntervalCounter = 0;
        mPauseOffset = 0;
        mIsPaused = false;
    }

    void incrementIntervalCounter() { mIntervalCounter++; }

    void pause(uint32_t now) {
        if (!mIsPaused) {
            mPauseTime = now;
            mIsPaused = true;
        }
    }

    void resume(uint32_t now) {
        if (mIsPaused) {
            mPauseOffset += now - mPauseTime;
            mIsPaused = false;
        }
    }

    // Gets the current frame and the next frame number based on the current time.
    void get_interval_frames(uint32_t now, uint32_t* frameNumber, uint32_t* nextFrameNumber, uint8_t* amountOfNextFrame = nullptr) const {
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

    uint32_t get_exact_timestamp_ms(uint32_t frameNumber) const {
        return mStartTime + (frameNumber * mMicrosSecondsPerInterval) / 1000;
    }

    bool isPaused() const { return mIsPaused; }

  private:
    uint64_t mMicrosSecondsPerInterval;
    uint32_t mIntervalCounter;
    uint32_t mStartTime;
    uint32_t mPauseOffset;
    uint32_t mPauseTime;
    bool mIsPaused;
};

FASTLED_NAMESPACE_END
