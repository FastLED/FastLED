#pragma once

#include "fl/stdint.h"

#include "fl/deprecated.h"
#include "fl/namespace.h"
#include "fl/memory.h"

namespace fl {

FASTLED_SMART_PTR(TimeFunction);
FASTLED_SMART_PTR(TimeWarp);

// Interface for time generation and time manipulation.
class TimeFunction {
  public:
    virtual ~TimeFunction() {}
    virtual fl::u32
    update(fl::u32 timeNow) = 0; // Inputs the real clock time and outputs the
                                  // virtual time.
    virtual fl::u32 time() const = 0;
    virtual void reset(fl::u32 realTimeNow) = 0;
};

// Time clock. Use this to gracefully handle time manipulation. You can input a
// float value representing the current time scale and this will adjust a the
// clock smoothly. Updating requires inputing the real clock from the millis()
// function. HANDLES NEGATIVE TIME SCALES!!! Use this to control viusualizers
// back and forth motion which draw according to a clock value. Clock will never
// go below 0.
class TimeWarp : public TimeFunction {
  public:
    TimeWarp(fl::u32 realTimeNow = 0, float initialTimeScale = 1.0f);
    ~TimeWarp();
    void setSpeed(float speedScale);
    void setScale(float speed)
        FASTLED_DEPRECATED("Use setSpeed(...) instead."); // Deprecated
    float scale() const;
    fl::u32 update(fl::u32 timeNow) override;
    fl::u32 time() const override;
    void reset(fl::u32 realTimeNow) override;
    void pause(fl::u32 now);
    void resume(fl::u32 now);

  private:
    void applyExact(fl::u32 timeNow);
    fl::u32 mLastRealTime = 0;
    fl::u32 mStartTime = 0;
    fl::u32 mRelativeTime = 0;
    float mTimeScale = 1.0f;
    fl::u32 mPauseTime = 0;
};

} // namespace fl
