#pragma once
#include "fl/stl/noexcept.h"

namespace fl {
namespace audio {

class Context;

class Detector {
public:
    virtual ~Detector() FL_NO_EXCEPT = default;

    virtual void update(shared_ptr<Context> context) FL_NO_EXCEPT = 0;
    virtual void fireCallbacks() FL_NO_EXCEPT {}
    virtual void setSampleRate(int) FL_NO_EXCEPT {}
    virtual bool needsFFT() const FL_NO_EXCEPT { return false; }
    virtual bool needsFFTHistory() const FL_NO_EXCEPT { return false; }
    virtual const char* getName() const FL_NO_EXCEPT = 0;
    virtual void reset() FL_NO_EXCEPT {}
};

} // namespace audio
} // namespace fl
