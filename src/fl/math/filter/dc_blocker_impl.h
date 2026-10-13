#pragma once
#include "fl/stl/noexcept.h"

namespace fl {
namespace detail {

// DC offset removal filter.
// y[n] = x[n] - x[n-1] + R * y[n-1]
// R controls the cutoff: closer to 1.0 = lower cutoff frequency.
// Default R = 0.995 gives ~1.6 Hz cutoff at 1 kHz sample rate.
template <typename T>
class DCBlockerImpl {
  public:
    explicit DCBlockerImpl(T r = T(0.995f))
        FL_NO_EXCEPT : mR(r), mX1(T(0)), mY(T(0)) {}

    T update(T input) FL_NO_EXCEPT {
        T output = input - mX1 + mR * mY;
        mX1 = input;
        mY = output;
        return output;
    }

    void setR(T r) FL_NO_EXCEPT { mR = r; }
    T value() const FL_NO_EXCEPT { return mY; }
    void reset() FL_NO_EXCEPT { mX1 = mY = T(0); }

  private:
    T mR;
    T mX1;
    T mY;
};

} // namespace detail
} // namespace fl
