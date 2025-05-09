
#pragma once

#include <stdint.h>

#include "fl/math_macros.h"
#include "fl/warn.h"

namespace fl {

// Use this function to compute the alpha value based on the time elapsed
// 0 -> 255
uint8_t time_alpha8(uint32_t now, uint32_t start, uint32_t end);
// 0 -> 65535
uint16_t time_alpha16(uint32_t now, uint32_t start, uint32_t end);

inline float time_alphaf(uint32_t now, uint32_t start, uint32_t end) {
    if (now < start) {
        return 0.0f;
    }
    uint32_t elapsed = now - start;
    uint32_t total = end - start;
    float out = static_cast<float>(elapsed) / static_cast<float>(total);
    return out;
}

class TimeAlpha {
  public:
    virtual ~TimeAlpha() = default;
    virtual void trigger(uint32_t now) = 0;
    virtual uint8_t update8(uint32_t now) = 0;
    virtual uint16_t update16(uint32_t now) {
        return static_cast<uint16_t>(update8(now) << 8) + 0xFF;
    }
    virtual float updatef(uint32_t now) {
        return static_cast<float>(update16(now)) / 65535.0f;
    }
    virtual bool isActive(uint32_t now) const = 0;
};

/*
 *                       amplitude
 *                          ^
 *  255 ───────────────────────
 *                     /        \
 *                    /          \
 *                   /            \
 *                  /              \
 *    0 ────────────┴               ┴──────────────────> time (ms)
 *                  t0   t1     t2   t4
 *
 *
 *
 */
class TimeRamp : public TimeAlpha {
  public:
    /// @param latchMs     total active time (ms)
    /// @param risingTime  time to ramp from 0→255 (ms)
    /// @param fallingTime time to ramp from 255→0 (ms)
    TimeRamp(uint32_t risingTime, uint32_t latchMs, uint32_t fallingTime);

    /// Call this when you want to (re)start the ramp cycle.
    void trigger(uint32_t now) override;

    void trigger(uint32_t now, uint32_t risingTime, uint32_t latchMs,
                 uint32_t fallingTime);

    /// @return true iff we're still within the latch period.
    bool isActive(uint32_t now) const override;

    /// Compute current 0–255 output based on how much time has elapsed since
    /// trigger().
    uint8_t update8(uint32_t now) override;

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

/*
 *                       amplitude
 *                          ^
 *  255 ──────────────────────────────────────
 *                     /
 *                    /
 *                   /
 *                  /
 *    0 ────────────┴                       --> time (ms)
 *                  t0   t1
 *
 *
 *
 */
class TimeClampedTransition : public TimeAlpha {
  public:
    TimeClampedTransition(uint32_t duration) : mDuration(duration) {}

    void trigger(uint32_t now) override {
        mStart = now;
        mEnd = now + mDuration;
    }

    bool isActive(uint32_t now) const override {
        bool not_started = (mEnd == 0) && (mStart == 0);
        if (not_started) {
            // if we have not started, we are not active
            return false;
        }
        if (now < mStart) {
            // if the time is before the start, we are not active
            return false;
        }
        if (now > mEnd) {
            // if the time is after the finished rising, we are not active
            return false;
        }
        return true;
    }

    uint8_t update8(uint32_t now) override {
        bool not_started = (mEnd == 0) && (mStart == 0);
        if (not_started) {
            // if we have not started, we are not active
            return 0;
        }
        uint8_t out = time_alpha8(now, mStart, mEnd);
        return out;
    }

    float updatef(uint32_t now) override {
        bool not_started = (mEnd == 0) && (mStart == 0);
        if (not_started) {
            return 0;
        }
        float out = time_alphaf(now, mStart, mEnd);
        if (mMaxClamp > 0.f) {
            out = MIN(out, mMaxClamp);
        }
        return out;
    }

    void set_max_clamp(float max) { mMaxClamp = max; }

  private:
    uint32_t mStart = 0;
    uint32_t mDuration = 0;
    uint32_t mEnd = 0;
    float mMaxClamp = -1.f; // default disabled.
};

} // namespace fl