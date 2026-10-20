#pragma once

namespace fl {
namespace detail {

template <typename T>
class KalmanFilterImpl {
  public:
    KalmanFilterImpl(T process_noise, T measurement_noise, T initial = T(0))
        : mQ(process_noise), mR(measurement_noise),
          mX(initial), mP(T(1.0f)), mLastValue(initial) {}

    T update(T measurement) {
        mP = mP + mQ;
        T k = mP / (mP + mR);
        mX = mX + k * (measurement - mX);
        mP = (T(1.0f) - k) * mP;
        mLastValue = mX;
        return mX;
    }

    T value() const { return mLastValue; }

    void reset(T initial = T(0)) {
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
