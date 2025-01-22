#pragma once

#include <stdint.h>

#include "fl/ptr.h"
#include "fl/callback.h"
#include "fl/namespace.h"

namespace fl {

FASTLED_SMART_PTR(TimeFunction);
FASTLED_SMART_PTR(TimeScale);


// Interface for time generation and time manipulation.
class TimeFunction: public fl::Referent {
  public:
    virtual ~TimeFunction() {}
    virtual uint32_t update(uint32_t timeNow) = 0;  // Inputs the real clock time and outputs the virtual time.
    virtual uint32_t time() const = 0;
    virtual void reset(uint32_t realTimeNow) = 0;
};

// Time clock, but you can warp time. Scale can go negative for back and
// forth time based effects. Starts at 0.
class TimeScale: public TimeFunction {
  public:
    TimeScale(uint32_t realTimeNow, float initialTimeScale = 1.0f);
    ~TimeScale();
    void setScale(float timeScale);
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

}  // namespace fl
