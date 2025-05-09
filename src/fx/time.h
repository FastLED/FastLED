#pragma once

#include <stdint.h>

#include "fl/deprecated.h"
#include "fl/namespace.h"
#include "fl/ptr.h"

namespace fl {

FASTLED_SMART_PTR(TimeFunction);
FASTLED_SMART_PTR(TimeWarp);

// Interface for time generation and time manipulation.
class TimeFunction : public fl::Referent {
  public:
    virtual ~TimeFunction() {}
    virtual uint32_t
    update(uint32_t timeNow) = 0; // Inputs the real clock time and outputs the
                                  // virtual time.
    virtual uint32_t time() const = 0;
    virtual void reset(uint32_t realTimeNow) = 0;
};

// Time clock. Use this to gracefully handle time manipulation. You can input a
// float value representing the current time scale and this will adjust a the
// clock smoothly. Updating requires inputing the real clock from the millis()
// function. HANDLES NEGATIVE TIME SCALES!!! Use this to control viusualizers
// back and forth motion which draw according to a clock value. Clock will never
// go below 0.
class TimeWarp : public TimeFunction {
  public:
    TimeWarp(uint32_t realTimeNow = 0, float initialTimeScale = 1.0f);
    ~TimeWarp();
    void setSpeed(float speedScale);
    void setScale(float speed)
        FASTLED_DEPRECATED("Use setSpeed(...) instead."); // Deprecated
    float scale() const;
    uint32_t update(uint32_t timeNow) override;
    uint32_t time() const override;
    void reset(uint32_t realTimeNow) override;
    void pause(uint32_t now);
    void resume(uint32_t now);

  private:
    void applyExact(uint32_t timeNow);
    uint32_t mLastRealTime = 0;
    uint32_t mStartTime = 0;
    uint32_t mRelativeTime = 0;
    float mTimeScale = 1.0f;
    uint32_t mPauseTime = 0;
};

} // namespace fl
