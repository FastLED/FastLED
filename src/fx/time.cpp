#include "time.h"

#include "namespace.h"

#include "fl/dbg.h"

#define DBG FASTLED_DBG

FASTLED_NAMESPACE_BEGIN

TimeScale::TimeScale(uint32_t realTimeNow, float initialTimeScale)
    : mLastRealTime(realTimeNow),
      mStartTime(realTimeNow),
      mTimeScale(initialTimeScale) {}

TimeScale::~TimeScale() {}

void TimeScale::setScale(float timeScale) {
    mTimeScale = timeScale;
}

float TimeScale::scale() const {
    return mTimeScale;
}

uint32_t TimeScale::update(uint32_t timeNow) {
    applyExact(timeNow);
    return time();
}

uint32_t TimeScale::time() const {
    return mRelativeTime;
}

void TimeScale::reset(uint32_t realTimeNow) {
    mLastRealTime = realTimeNow;
    mStartTime = realTimeNow;
    mRelativeTime = 0;
}

void TimeScale::applyExact(uint32_t timeNow) {
    //DBG("TimeScale::applyExact: timeNow: " << timeNow << " mLastRealTime: " << mLastRealTime);
    if (mLastRealTime > timeNow) {
        DBG("TimeScale::applyExact: mLastRealTime > timeNow");
        return;
    }
    uint32_t elapsedRealTime = timeNow - mLastRealTime;
    mLastRealTime = timeNow;
    int32_t diff = static_cast<int32_t>(elapsedRealTime * mTimeScale);
    if (diff == 0) {
        return;
    }
    if (diff >= 0) {
        mRelativeTime += diff;
        return;
    }

    // diff < 0
    uint32_t abs_diff = -diff;
    if (abs_diff > mRelativeTime) {
        mRelativeTime = 0;
        return;
    }
    mLastRealTime = timeNow;
    mRelativeTime -= abs_diff;
}

FASTLED_NAMESPACE_END