#pragma once
#include "fl/stl/noexcept.h"

namespace fl {
namespace audio {

class Context;

class Detector {
public:
    virtual ~Detector() FL_NOEXCEPT = default;

    virtual void update(shared_ptr<Context> context) FL_NOEXCEPT = 0;
    virtual void fireCallbacks() FL_NOEXCEPT {}
    virtual void setSampleRate(int) FL_NOEXCEPT {}
    virtual bool needsFFT() const FL_NOEXCEPT { return false; }
    virtual bool needsFFTHistory() const FL_NOEXCEPT { return false; }
    virtual const char* getName() const FL_NOEXCEPT = 0;
    virtual void reset() FL_NOEXCEPT {}
};

} // namespace audio
} // namespace fl
