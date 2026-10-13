#pragma once
#include "fl/stl/noexcept.h"

namespace fl {
namespace detail {

template <typename T>
class KalmanFilterImpl {
  public:
    KalmanFilterImpl(T process_noise, T measurement_noise, T initial = T(0))
        FL_NO_EXCEPT : mQ(process_noise), mR(measurement_noise),
          mX(initial), mP(T(1.0f)), mLastValue(initial) {}

    T update(T measurement) FL_NO_EXCEPT {
        mP = mP + mQ;
        T k = mP / (mP + mR);
        mX = mX + k * (measurement - mX);
        mP = (T(1.0f) - k) * mP;
        mLastValue = mX;
        return mX;
    }

    T value() const FL_NO_EXCEPT { return mLastValue; }

    void reset(T initial = T(0)) FL_NO_EXCEPT {
        mX = initial;
        mP = T(1.0f);
        mLastValue = initial;
    }

  private:
    T mQ;
    T mR;
    T mX;
    T mP;
    T mLastValue;
};

} // namespace detail
} // namespace fl
