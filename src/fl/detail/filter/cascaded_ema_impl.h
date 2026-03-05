#pragma once

#include "fl/stl/math.h"

namespace fl {
namespace detail {

template <typename T, int Stages = 2>
class CascadedEMAImpl {
  public:
    explicit CascadedEMAImpl(T tau_seconds, T initial = T(0))
        : mTau(tau_seconds) {
        for (int i = 0; i < Stages; ++i) {
            mY[i] = initial;
        }
    }

    T update(T input, T dt_seconds) {
        T decay = fl::exp(-(dt_seconds / mTau));
        T val = input;
        for (int i = 0; i < Stages; ++i) {
            mY[i] = val + (mY[i] - val) * decay;
            val = mY[i];
        }
        return val;
    }

    T value() const { return mY[Stages - 1]; }
    void setTau(T tau_seconds) { mTau = tau_seconds; }

    void reset(T initial = T(0)) {
        for (int i = 0; i < Stages; ++i) mY[i] = initial;
    }

  private:
    T mTau;
    T mY[Stages];
};

} // namespace detail
} // namespace fl
