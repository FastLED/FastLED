#pragma once

#include "fl/stl/math.h"

namespace fl {
namespace detail {

template <typename T>
class ExponentialSmootherImpl {
  public:
    explicit ExponentialSmootherImpl(T tau_seconds, T initial = T(0))
        : mTau(tau_seconds), mY(initial) {}

    T update(T input, T dt_seconds) {
        if (mTau <= T(0)) {
            mY = input;  // No smoothing when tau <= 0
            return mY;
        }
        T decay = fl::exp(-(dt_seconds / mTau));
        mY = input + (mY - input) * decay;
        return mY;
    }

    void setTau(T tau_seconds) { mTau = tau_seconds; }
    T value() const { return mY; }
    void reset(T initial = T(0)) { mY = initial; }

  private:
    T mTau;
    T mY;
};

} // namespace detail
} // namespace fl
