#include "time.h"

#include "fl/namespace.h"

#include "fl/dbg.h"
#include "fl/warn.h"

#define DBG FASTLED_DBG

namespace fl {

TimeWarp::TimeWarp(fl::u32 realTimeNow, float initialTimeScale)
    : mLastRealTime(realTimeNow), mStartTime(realTimeNow),
      mTimeScale(initialTimeScale) {}

TimeWarp::~TimeWarp() {}

void TimeWarp::setSpeed(float timeScale) { mTimeScale = timeScale; }

float TimeWarp::scale() const { return mTimeScale; }

void TimeWarp::pause(fl::u32 now) {
    if (mPauseTime) {
        FASTLED_WARN("TimeWarp::pause: already paused");
        return;
    }
    mPauseTime = now;
}
void TimeWarp::resume(fl::u32 now) {
    if (mLastRealTime == 0) {
        reset(now);
        return;
    }
    fl::u32 diff = now - mPauseTime;
    mStartTime += diff;
    mLastRealTime += diff;
    mPauseTime = 0;
}

fl::u32 TimeWarp::update(fl::u32 timeNow) {

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

fl::u32 TimeWarp::time() const { return mRelativeTime; }

void TimeWarp::reset(fl::u32 realTimeNow) {
    mLastRealTime = realTimeNow;
    mStartTime = realTimeNow;
    mRelativeTime = 0;
}

void TimeWarp::applyExact(fl::u32 timeNow) {
    fl::u32 elapsedRealTime = timeNow - mLastRealTime;
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
    fl::u32 abs_diff = -diff;
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
