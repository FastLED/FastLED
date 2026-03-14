#pragma once

#include "fl/stl/math.h"

namespace fl {
namespace detail {

template <typename T>
class OneEuroFilterImpl {
  public:
    OneEuroFilterImpl(T min_cutoff, T beta, T d_cutoff = T(1.0f))
        : mMinCutoff(min_cutoff), mBeta(beta), mDCutoff(d_cutoff),
          mX(T(0)), mDX(T(0)), mFirst(true), mLastValue(T(0)) {}

    T update(T input, T dt) {
        if (mFirst) {
            mX = input;
            mDX = T(0);
            mFirst = false;
            mLastValue = input;
            return input;
        }
        if (dt == T(0)) {
            return mLastValue;
        }
        T dx = (input - mX) / dt;
        T alpha_d = computeAlpha(mDCutoff, dt);
        mDX = mDX + alpha_d * (dx - mDX);
        T cutoff = mMinCutoff + mBeta * fl::abs(mDX);
        T alpha = computeAlpha(cutoff, dt);
        mX = mX + alpha * (input - mX);
        mLastValue = mX;
        return mX;
    }

    T value() const { return mLastValue; }

    void reset(T initial = T(0)) {
        mX = initial;
        mDX = T(0);
        mFirst = true;
        mLastValue = initial;
    }

  private:
    static T computeAlpha(T cutoff, T dt) {
        T tau = T(static_cast<float>(2.0 * FL_PI)) * cutoff * dt;
        return tau / (T(1.0f) + tau);
    }

    T mMinCutoff;
    T mBeta;
    T mDCutoff;
    T mX;
    T mDX;
    bool mFirst;
    T mLastValue;
};

} // namespace detail
} // namespace fl
