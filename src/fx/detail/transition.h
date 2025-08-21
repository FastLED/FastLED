#pragma once

#include "fl/namespace.h"
#include "fl/stdint.h"
#include "fl/int.h"

namespace fl {

// Logic to control the progression of a transition over time.
class Transition {
  public:
    Transition() : mStart(0), mDuration(0), mNotStarted(true) {}
    ~Transition() {}

    uint8_t getProgress(fl::u32 now) {
        if (mNotStarted) {
            return 0;
        }
        if (now < mStart) {
            return 0;
        } else if (now >= mStart + mDuration) {
            return 255;
        } else {
            return ((now - mStart) * 255) / mDuration;
        }
    }

    void start(fl::u32 now, fl::u32 duration) {
        mNotStarted = false;
        mStart = now;
        mDuration = duration;
    }

    void end() { mNotStarted = true; }

    bool isTransitioning(fl::u32 now) {
        if (mNotStarted) {
            return false;
        }
        return now >= mStart && now < mStart + mDuration;
    }

  private:
    fl::u32 mStart;
    fl::u32 mDuration;
    bool mNotStarted;
};

} // namespace fl
