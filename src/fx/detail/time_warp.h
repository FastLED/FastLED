#pragma once

#include <stdint.h>

#include "namespace.h"

FASTLED_NAMESPACE_BEGIN

// This class keeps track of the current time and modifies it to allow for time
// warping effects. You will get the warped time value by calling getTime().
class TimeWarp {
  public:
    enum MODE {
        EXACT,
    };
    TimeWarp(uint32_t realTimeNow, float initialTimeScale = 1.0f)
        : mRealTime(realTimeNow), mLastRealTime(realTimeNow),
          mStartTime(realTimeNow), mTimeScale(initialTimeScale) {}
    void setTimeScale(float timeScale) { mTimeScale = timeScale; }
    float getTimeScale() const { return mTimeScale; }
    void update(uint32_t timeNow) {
        switch (mMode) {
        case EXACT:
            applyExact(timeNow);
            break;
        }
    }

    uint32_t getTime() const { return mRealTime; }
    uint32_t reset(uint32_t timeNow, float timeScale = 1.0f) {
        mRealTime = timeNow;
        mLastRealTime = timeNow;
        mStartTime = timeNow;
        mTimeScale = timeScale;
        return mRealTime;
    }

    void setMode(MODE mode) { mMode = mode; }

  private:
    void applyExact(uint32_t timeNow) {
        uint32_t elapsedRealTime = timeNow - mLastRealTime;
        int32_t diff = static_cast<int32_t>(elapsedRealTime * mTimeScale);
        if (diff < 0) {
            if (mRealTime + diff < mStartTime) {
                mRealTime = mStartTime;
                mLastRealTime = mStartTime;
                return;
            }
            uint32_t newRealTime = mRealTime + diff;
            if (newRealTime > mRealTime) {
                // rolled over to positive territory.
                mRealTime = mStartTime;
                mLastRealTime = mStartTime;
                return;
            }
        }
        mRealTime += diff;
        mLastRealTime = timeNow;
    }

    uint32_t mRealTime = 0;
    uint32_t mLastRealTime = 0;
    uint32_t mStartTime = 0;
    float mTimeScale = 1.0f;
    MODE mMode = EXACT;
};

FASTLED_NAMESPACE_END
