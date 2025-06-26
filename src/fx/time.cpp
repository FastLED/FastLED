#include "time.h"

#include "fl/namespace.h"

#include "fl/dbg.h"
#include "fl/warn.h"

#define DBG FASTLED_DBG

namespace fl {

TimeWarp::TimeWarp(uint32_t realTimeNow, float initialTimeScale)
    : mLastRealTime(realTimeNow), mStartTime(realTimeNow),
      mTimeScale(initialTimeScale) {}

TimeWarp::~TimeWarp() {}

void TimeWarp::setSpeed(float timeScale) { mTimeScale = timeScale; }

float TimeWarp::scale() const { return mTimeScale; }

void TimeWarp::pause(uint32_t now) {
    if (mPauseTime) {
        FASTLED_WARN("TimeWarp::pause: already paused");
        return;
    }
    mPauseTime = now;
}
void TimeWarp::resume(uint32_t now) {
    if (mLastRealTime == 0) {
        reset(now);
        return;
    }
    uint32_t diff = now - mPauseTime;
    mStartTime += diff;
    mLastRealTime += diff;
    mPauseTime = 0;
}

uint32_t TimeWarp::update(uint32_t timeNow) {

    // DBG("TimeWarp::update: timeNow: " << timeNow << " mLastRealTime: " <<
    // mLastRealTime
    //<< " mRelativeTime: " << mRelativeTime << " mTimeScale: " << mTimeScale);

    if (mLastRealTime > timeNow) {
        DBG("TimeWarp::applyExact: mLastRealTime > timeNow: "
            << mLastRealTime << " > " << timeNow);
    }

    applyExact(timeNow);
    return time();
}

uint32_t TimeWarp::time() const { return mRelativeTime; }

void TimeWarp::reset(uint32_t realTimeNow) {
    mLastRealTime = realTimeNow;
    mStartTime = realTimeNow;
    mRelativeTime = 0;
}

void TimeWarp::applyExact(uint32_t timeNow) {
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

void TimeWarp::setScale(float speed) { mTimeScale = speed; }

} // namespace fl