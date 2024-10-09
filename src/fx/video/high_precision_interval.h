#pragma once

#include "namespace.h"

FASTLED_NAMESPACE_BEGIN

class HighPrecisionInterval {
public:
    HighPrecisionInterval(uint64_t microsSecondsPerInterval)
        : mMicrosSecondsPerInterval(microsSecondsPerInterval), mIntervalCounter(0), mStartTime(0), mPauseOffset(0), mPauseTime(0), mIsPaused(false) {}

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

    bool needsRefresh(uint32_t now, uint32_t* precise_timestamp) const {
        if (mIsPaused) {
            return false;
        }

        uint32_t adjustedNow = now - mPauseOffset;
        uint32_t elapsed = adjustedNow - mStartTime;
        uint32_t elapsedMicros = elapsed * 1000;
        uint32_t intervalNumber = elapsedMicros / mMicrosSecondsPerInterval;
        bool needs_update = intervalNumber > mIntervalCounter;
        if (needs_update) {
            *precise_timestamp = mStartTime + ((mIntervalCounter+1) * mMicrosSecondsPerInterval) / 1000 + mPauseOffset;
        }
        return needs_update;
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
