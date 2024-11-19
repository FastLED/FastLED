#include "time.h"

#include "namespace.h"

FASTLED_NAMESPACE_BEGIN

TimeScale::TimeScale(uint32_t realTimeNow, float initialTimeScale)
    : mRealTime(realTimeNow), mLastRealTime(realTimeNow),
        mStartTime(realTimeNow), mTimeScale(initialTimeScale) {}

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
    return mRealTime;
}

void TimeScale::reset(uint32_t realTimeNow) {
    mRealTime = realTimeNow;
    mLastRealTime = realTimeNow;
    mStartTime = realTimeNow;
}

void TimeScale::applyExact(uint32_t timeNow) {
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

FASTLED_NAMESPACE_END