#pragma once

#include <stdint.h>

#include "ref.h"
#include "callback.h"
#include "namespace.h"

FASTLED_NAMESPACE_BEGIN

FASTLED_SMART_REF(TimeFunction);
FASTLED_SMART_REF(TimeScale);


// Interface for time generation and time manipulation.
class TimeFunction: public Referent {
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
  private:
    void applyExact(uint32_t timeNow);
    uint32_t mRealTime = 0;
    uint32_t mLastRealTime = 0;
    uint32_t mStartTime = 0;
    float mTimeScale = 1.0f;
};

FASTLED_NAMESPACE_END
