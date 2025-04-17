
#pragma once

#include "fl/transition_ramp.h"
#include "fl/warn.h"
#include "math_macros.h"

namespace fl {

RampTimer::RampTimer(uint32_t latchMs, uint32_t risingTime,
                     uint32_t fallingTime)
    : mLatchMs(latchMs), mRisingTime(risingTime), mFallingTime(fallingTime) {
    // enforce rising+fall ≤ latch
    if (mRisingTime + mFallingTime > mLatchMs) {
        mRisingTime = mLatchMs / 2;
        mFallingTime = mLatchMs / 2;
    }
}

void RampTimer::trigger(uint32_t now) {
    mStart = now;
    mLastValue = 0;
}

bool RampTimer::isActive(uint32_t now) const {
    return (now - mStart) < mLatchMs;
}

uint8_t RampTimer::value(uint32_t now) {
    uint32_t elapsed = now - mStart;
    uint8_t out = 0;

    if (elapsed < mRisingTime) {
        // rising: 0 → 255
        uint8_t v = (elapsed * 255) / mRisingTime;
        out = MAX(mLastValue, v);
    } else if (elapsed < (mLatchMs - mFallingTime)) {
        // full‑on plateau
        out = 255;
    } else if (elapsed < mLatchMs) {
        // falling: 255 → 0
        uint32_t t = elapsed - (mLatchMs - mFallingTime);
        out = 255 - ((t * 255) / mFallingTime);
    }
    // else: elapsed ≥ latch → out = 0

    mLastValue = out;
    return out;
}

} // namespace fl