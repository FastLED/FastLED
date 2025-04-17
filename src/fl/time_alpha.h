
#pragma once

#include <stdint.h>

namespace fl {

class TimeRamp {
  public:
    static uint8_t timeAlpha(uint32_t now, uint32_t start, uint32_t end);

    /// @param latchMs     total active time (ms)
    /// @param risingTime  time to ramp from 0→255 (ms)
    /// @param fallingTime time to ramp from 255→0 (ms)
    TimeRamp(uint32_t risingTime, uint32_t latchMs, uint32_t fallingTime);

    /// Call this when you want to (re)start the ramp cycle.
    void trigger(uint32_t now);

    void trigger(uint32_t now, uint32_t risingTime, uint32_t latchMs,
                 uint32_t fallingTime);

    /// @return true iff we're still within the latch period.
    bool isActive(uint32_t now) const;

    /// Compute current 0–255 output based on how much time has elapsed since
    /// trigger().
    uint8_t update(uint32_t now);

  private:
    uint32_t mLatchMs;
    uint32_t mRisingTime;
    uint32_t mFallingTime;

    uint32_t mFinishedRisingTime = 0;
    uint32_t mFinishedPlateauTime = 0;
    uint32_t mFinishedFallingTime = 0;

    uint32_t mStart = 0;
    uint8_t mLastValue = 0;
};

class TimeLinear {
  public:
    TimeLinear(uint32_t duration) : mDuration(duration) {}

    void trigger(uint32_t now) {
        mStart = now;
        mEnd = now + mDuration;
    }

    uint8_t update(uint32_t now) {
        bool not_started = (mEnd == 0) && (mStart == 0);
        if (not_started) {
            // if we have not started, we are not active
            return 0;
        }
        uint8_t out = TimeRamp::timeAlpha(now, mStart, mEnd);
        return out;
    }

  private:
    uint32_t mStart = 0;
    uint32_t mDuration = 0;
    uint32_t mEnd = 0;
};

} // namespace fl