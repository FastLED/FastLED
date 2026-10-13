#pragma once

#include "fl/math/math.h"
#include "fl/stl/noexcept.h"

namespace fl {
namespace detail {

template <typename T>
class ExponentialSmootherImpl {
  public:
    explicit ExponentialSmootherImpl(T tau_seconds, T initial = T(0))
        FL_NO_EXCEPT : mTau(tau_seconds), mY(initial) {}

    T update(T input, T dt_seconds) FL_NO_EXCEPT {
        if (mTau <= T(0)) {
            mY = input;  // No smoothing when tau <= 0
            return mY;
        }
        T decay = fl::exp(-(dt_seconds / mTau));
        mY = input + (mY - input) * decay;
        return mY;
    }

    void setTau(T tau_seconds) FL_NO_EXCEPT { mTau = tau_seconds; }
    T value() const FL_NO_EXCEPT { return mY; }
    void reset(T initial = T(0)) FL_NO_EXCEPT { mY = initial; }

  private:
    T mTau;
    T mY;
};

} // namespace detail
} // namespace fl
