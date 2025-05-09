
#include "fl/time_alpha.h"
#include "fl/warn.h"
#include "math_macros.h"

namespace fl {

uint8_t time_alpha8(uint32_t now, uint32_t start, uint32_t end) {
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

uint16_t time_alpha16(uint32_t now, uint32_t start, uint32_t end) {
    if (now < start) {
        return 0;
    }
    if (now > end) {
        return 65535;
    }
    uint32_t elapsed = now - start;
    uint32_t total = end - start;
    uint32_t out = (elapsed * 65535) / total;
    if (out > 65535) {
        out = 65535;
    }
    return static_cast<uint16_t>(out);
}

TimeRamp::TimeRamp(uint32_t risingTime, uint32_t latchMs, uint32_t fallingTime)
    : mLatchMs(latchMs), mRisingTime(risingTime), mFallingTime(fallingTime) {}

void TimeRamp::trigger(uint32_t now) {
    mStart = now;
    // mLastValue = 0;

    mFinishedRisingTime = mStart + mRisingTime;
    mFinishedPlateauTime = mFinishedRisingTime + mLatchMs;
    mFinishedFallingTime = mFinishedPlateauTime + mFallingTime;
}

void TimeRamp::trigger(uint32_t now, uint32_t risingTime, uint32_t latchMs,
                       uint32_t fallingTime) {
    mRisingTime = risingTime;
    mLatchMs = latchMs;
    mFallingTime = fallingTime;
    trigger(now);
}

bool TimeRamp::isActive(uint32_t now) const {

    bool not_started = (mFinishedRisingTime == 0) &&
                       (mFinishedPlateauTime == 0) &&
                       (mFinishedFallingTime == 0);
    if (not_started) {
        // if we have not started, we are not active
        return false;
    }

    if (now < mStart) {
        // if the time is before the start, we are not active
        return false;
    }

    if (now > mFinishedFallingTime) {
        // if the time is after the finished rising, we are not active
        return false;
    }

    return true;
}

uint8_t TimeRamp::update8(uint32_t now) {
    if (!isActive(now)) {
        return 0;
    }
    // uint32_t elapsed = now - mStart;
    uint8_t out = 0;
    if (now < mFinishedRisingTime) {
        out = time_alpha8(now, mStart, mFinishedRisingTime);
    } else if (now < mFinishedPlateauTime) {
        // plateau
        out = 255;
    } else if (now < mFinishedFallingTime) {
        // ramp down
        uint8_t alpha =
            time_alpha8(now, mFinishedPlateauTime, mFinishedFallingTime);
        out = 255 - alpha;
    } else {
        // finished
        out = 0;
    }

    mLastValue = out;
    return out;
}

} // namespace fl