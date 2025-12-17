#pragma once

#include "fl/stl/shared_ptr.h"

namespace fl {

class AudioContext;

class AudioDetector {
public:
    virtual ~AudioDetector() = default;

    virtual void update(shared_ptr<AudioContext> context) = 0;
    virtual bool needsFFT() const { return false; }
    virtual bool needsFFTHistory() const { return false; }
    virtual const char* getName() const = 0;
    virtual void reset() {}
};

} // namespace fl
