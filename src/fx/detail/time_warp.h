#pragma once

#include <stdint.h>

#include "namespace.h"


FASTLED_NAMESPACE_BEGIN

// This class keeps track of the current time and modifies it to allow for time warping effects.
// You will get the warped time value by calling getTime().
class TimeWarp {
public:
    TimeWarp(uint32_t realTimeNow, float initialTimeScale = 1.0f) : mRealTime(realTimeNow), mLastRealTime(realTimeNow), mTimeScale(initialTimeScale) {}
    void setTimeScale(float timeScale) { mTimeScale = timeScale; }
    float getTimeScale() const { return mTimeScale; }
    void update(uint32_t timeNow) {
        uint32_t elapsedRealTime = timeNow - mLastRealTime;
        mRealTime += static_cast<uint32_t>(elapsedRealTime * mTimeScale);
        mLastRealTime = timeNow;
    }
    
    uint32_t getTime() const {
        return mRealTime;
    }
    uint32_t reset(uint32_t timeNow) {
        mRealTime = 0;
        mLastRealTime = timeNow;
        return mRealTime;
    }

private:
    uint32_t mRealTime = 0;
    uint32_t mLastRealTime = 0;
    float mTimeScale = 1.0f;
};

FASTLED_NAMESPACE_END
