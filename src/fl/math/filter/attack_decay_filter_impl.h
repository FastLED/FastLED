#pragma once

#include "fl/math/math.h"

namespace fl {
namespace detail {

template <typename T>
class AttackDecayFilterImpl {
  public:
    AttackDecayFilterImpl(T attack_tau, T decay_tau, T initial = T(0))
        : mAttackTau(attack_tau), mDecayTau(decay_tau), mY(initial) {}

    T update(T input, T dt_seconds) {
        T abs_input = (input < T(0)) ? -input : input;
        T abs_y     = (mY < T(0))    ? -mY    : mY;
        T tau = (abs_input > abs_y) ? mAttackTau : mDecayTau;
        if (tau <= T(0)) {
            mY = input;  // No smoothing when tau <= 0
            return mY;
        }
        T decay = fl::exp(-(dt_seconds / tau));
        mY = input + (mY - input) * decay;
        return mY;
    }

    void setAttackTau(T tau_seconds) { mAttackTau = tau_seconds; }
    void setDecayTau(T tau_seconds) { mDecayTau = tau_seconds; }
    T value() const { return mY; }
    void reset(T initial = T(0)) { mY = initial; }

  private:
    T mAttackTau;
    T mDecayTau;
    T mY;
};

} // namespace detail
} // namespace fl
