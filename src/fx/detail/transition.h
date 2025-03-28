#pragma once

#include <stdint.h>
#include "fl/namespace.h"

namespace fl {

// Logic to control the progression of a transition over time.
class Transition {
public:
    Transition() : mStart(0), mDuration(0), mNotStarted(true) {}
    ~Transition() {}

    uint8_t getProgress(uint32_t now) {
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

    void start(uint32_t now, uint32_t duration) {
        mNotStarted = false;
        mStart = now;
        mDuration = duration;
    }

    void end() {
        mNotStarted = true;
    }

    bool isTransitioning(uint32_t now) {
        if (mNotStarted) {
            return false;
        }   
        return now >= mStart && now < mStart + mDuration;
    }

private:
    uint32_t mStart;
    uint32_t mDuration;
    bool mNotStarted;
};


}  // namespace fl

