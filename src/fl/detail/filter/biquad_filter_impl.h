#pragma once

#include "fl/math_macros.h"

namespace fl {
namespace detail {

template <typename T>
class BiquadFilterImpl {
  public:
    BiquadFilterImpl(T b0, T b1, T b2, T a1, T a2)
        : mB0(b0), mB1(b1), mB2(b2), mA1(a1), mA2(a2),
          mX1(T(0)), mX2(T(0)), mY1(T(0)), mY2(T(0)), mLastValue(T(0)) {}

    // Butterworth low-pass: -12 dB/octave roll-off above cutoff_hz.
    static BiquadFilterImpl butterworth(float cutoff_hz, float sample_rate) {
        float omega = 2.0f * static_cast<float>(FL_PI) * cutoff_hz / sample_rate;
        float sn = fl::sinf(omega);
        float cs = fl::cosf(omega);
        float alpha = sn * 0.7071067811865476f;
        float a0 = 1.0f + alpha;
        float b0f = (1.0f - cs) * 0.5f / a0;
        float b1f = (1.0f - cs) / a0;
        float b2f = b0f;
        float a1f = (-2.0f * cs) / a0;
        float a2f = (1.0f - alpha) / a0;
        return BiquadFilterImpl(T(b0f), T(b1f), T(b2f), T(a1f), T(a2f));
    }

    // Butterworth high-pass: -12 dB/octave roll-off below cutoff_hz.
    static BiquadFilterImpl highpass(float cutoff_hz, float sample_rate) {
        float omega = 2.0f * static_cast<float>(FL_PI) * cutoff_hz / sample_rate;
        float sn = fl::sinf(omega);
        float cs = fl::cosf(omega);
        float alpha = sn * 0.7071067811865476f;
        float a0 = 1.0f + alpha;
        float b0f = (1.0f + cs) * 0.5f / a0;
        float b1f = -(1.0f + cs) / a0;
        float b2f = b0f;
        float a1f = (-2.0f * cs) / a0;
        float a2f = (1.0f - alpha) / a0;
        return BiquadFilterImpl(T(b0f), T(b1f), T(b2f), T(a1f), T(a2f));
    }

    // Band-pass filter centered at center_hz with given Q factor.
    // Q controls bandwidth: higher Q = narrower band. Q=0.707 is broadband.
    static BiquadFilterImpl bandpass(float center_hz, float sample_rate,
                                     float q = 1.0f) {
        float omega = 2.0f * static_cast<float>(FL_PI) * center_hz / sample_rate;
        float sn = fl::sinf(omega);
        float cs = fl::cosf(omega);
        float alpha = sn / (2.0f * q);
        float a0 = 1.0f + alpha;
        float b0f = alpha / a0;
        float b1f = 0.0f;
        float b2f = -alpha / a0;
        float a1f = (-2.0f * cs) / a0;
        float a2f = (1.0f - alpha) / a0;
        return BiquadFilterImpl(T(b0f), T(b1f), T(b2f), T(a1f), T(a2f));
    }

    // Notch (band-reject) filter at center_hz with given Q factor.
    // Removes a narrow frequency band. Q=30 is good for 50/60 Hz hum.
    static BiquadFilterImpl notch(float center_hz, float sample_rate,
                                   float q = 1.0f) {
        float omega = 2.0f * static_cast<float>(FL_PI) * center_hz / sample_rate;
        float sn = fl::sinf(omega);
        float cs = fl::cosf(omega);
        float alpha = sn / (2.0f * q);
        float a0 = 1.0f + alpha;
        float b0f = 1.0f / a0;
        float b1f = (-2.0f * cs) / a0;
        float b2f = b0f;
        float a1f = b1f;
        float a2f = (1.0f - alpha) / a0;
        return BiquadFilterImpl(T(b0f), T(b1f), T(b2f), T(a1f), T(a2f));
    }

    T update(T input) {
        T output = mB0 * input + mB1 * mX1 + mB2 * mX2
                 - mA1 * mY1 - mA2 * mY2;
        mX2 = mX1;
        mX1 = input;
        mY2 = mY1;
        mY1 = output;
        mLastValue = output;
        return output;
    }

    T value() const { return mLastValue; }

    void reset() {
        mX1 = mX2 = mY1 = mY2 = mLastValue = T(0);
    }

  private:
    T mB0, mB1, mB2, mA1, mA2;
    T mX1, mX2;
    T mY1, mY2;
    T mLastValue;
};

} // namespace detail
} // namespace fl
