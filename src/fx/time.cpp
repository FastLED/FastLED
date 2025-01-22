#include "time.h"

#include "fl/namespace.h"

#include "fl/dbg.h"
#include "fl/warn.h"

#define DBG FASTLED_DBG

namespace fl {

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

void TimeScale::pause(uint32_t now) {
    if (mPauseTime) {
        FASTLED_WARN("TimeScale::pause: already paused");
        return;
    }
    mPauseTime = now;
}
void TimeScale::resume(uint32_t now) {
    if (mLastRealTime == 0) {
        reset(now);
        return;
    }
    uint32_t diff = now - mPauseTime;
    mStartTime += diff;
    mLastRealTime += diff;
    mPauseTime = 0;
}

uint32_t TimeScale::update(uint32_t timeNow) {

    //DBG("TimeScale::update: timeNow: " << timeNow << " mLastRealTime: " << mLastRealTime
    //<< " mRelativeTime: " << mRelativeTime << " mTimeScale: " << mTimeScale);

    if (mLastRealTime > timeNow) {
        DBG("TimeScale::applyExact: mLastRealTime > timeNow: " << mLastRealTime << " > " << timeNow);
    }

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
        // Protection against rollover.
        mRelativeTime = 0;
        mLastRealTime = timeNow;
        return;
    }
    mLastRealTime = timeNow;
    mRelativeTime -= abs_diff;
}

}  // namespace fl