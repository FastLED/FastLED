#pragma once

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
        : mR(r), mX1(T(0)), mY(T(0)) {}

    T update(T input) {
        T output = input - mX1 + mR * mY;
        mX1 = input;
        mY = output;
        return output;
    }

    void setR(T r) { mR = r; }
    T value() const { return mY; }
    void reset() { mX1 = mY = T(0); }

  private:
    T mR;
    T mX1;
    T mY;
};

} // namespace detail
} // namespace fl
