
#pragma once

#include <stdint.h>

namespace fl {

class TransitionRamp {
  public:
    /// @param latchMs     total active time (ms)
    /// @param risingTime  time to ramp from 0→255 (ms)
    /// @param fallingTime time to ramp from 255→0 (ms)
    TransitionRamp(uint32_t latchMs, uint32_t risingTime, uint32_t fallingTime);

    /// Call this when you want to (re)start the ramp cycle.
    void trigger(uint32_t now);

    /// @return true iff we're still within the latch period.
    bool isActive(uint32_t now) const;

    /// Compute current 0–255 output based on how much time has elapsed since
    /// trigger().
    uint8_t value(uint32_t now);

  private:
    uint32_t mLatchMs;
    uint32_t mRisingTime;
    uint32_t mFallingTime;

    uint32_t mStart = 0;
    uint8_t mLastValue = 0;
};

} // namespace fl