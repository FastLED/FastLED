
#include "fl/transition_ramp.h"
#include "fl/warn.h"
#include "math_macros.h"

namespace fl {


uint8_t TransitionRamp::timeAlpha(uint32_t now, uint32_t start, uint32_t end) {
    if (now < start) {
        return 0;
    }
    if (now > end) {
        return 255;
    }
    uint32_t elapsed = now - start;
    uint32_t total = end - start;
    uint32_t out = (elapsed * 255) / total;
    if (out > 255) {
        out = 255;
    }
    return static_cast<uint8_t>(out);
}


TransitionRamp::TransitionRamp(uint32_t risingTime, uint32_t latchMs,
                               uint32_t fallingTime)
    : mLatchMs(latchMs), mRisingTime(risingTime), mFallingTime(fallingTime) {}

void TransitionRamp::trigger(uint32_t now) {
    mActive = true;
    mStart = now;
    mLastValue = 0;

    mFinishedRisingTime = mStart + mRisingTime;
    mFinishedPlateauTime = mFinishedRisingTime + mLatchMs;
    mFinishedFallingTime = mFinishedPlateauTime + mFallingTime;
}

bool TransitionRamp::isActive(uint32_t now) const {
    if (!mActive) {
        return false;
    }

    if (now < mStart) {
        // if the time is before the start, we are not active
        return false;
    }

    if (now > mFinishedFallingTime) {
        // if the time is after the plateau, we are not active
        return false;
    }

    return true;
}



uint8_t TransitionRamp::update(uint32_t now) {
    if (!isActive(now)) {
        return 0;
    }
    // uint32_t elapsed = now - mStart;
    uint8_t out = 0;
    if (now < mFinishedRisingTime) {
        out = timeAlpha(now, mStart, mFinishedRisingTime);
    } else if (now < mFinishedPlateauTime) {
        // plateau
        out = 255;
    } else if (now < mFinishedFallingTime) {
        // ramp down
        uint8_t alpha =
            timeAlpha(now, mFinishedPlateauTime, mFinishedFallingTime);
        out = 255 - alpha;
    } else {
        // finished
        out = 0;
        mActive = false;
    }

    mLastValue = out;
    return out;
}

} // namespace fl