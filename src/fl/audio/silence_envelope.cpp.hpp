#pragma once

#include "fl/audio/silence_envelope.h"
#include "fl/math/math.h"

namespace fl {
namespace audio {

SilenceEnvelope::SilenceEnvelope() FL_NOEXCEPT : mConfig{}, mCurrent(0.0f) {}

SilenceEnvelope::SilenceEnvelope(const Config& cfg) FL_NOEXCEPT
    : mConfig(cfg), mCurrent(cfg.targetValue) {}

float SilenceEnvelope::update(bool isSilent, float currentValue, float dt) FL_NOEXCEPT {
    if (!isSilent) {
        // Pass-through during audio; cache for decay if silence starts next frame.
        mCurrent = currentValue;
        return mCurrent;
    }

    // During silence — exponential decay toward target with time constant tau.
    // alpha = 1 - exp(-dt/tau) gives a proper first-order response independent
    // of dt. At dt=tau, alpha ≈ 0.632; at dt=3*tau, we've crossed 95% of the way.
    if (mConfig.decayTauSeconds <= 0.0f || dt <= 0.0f) {
        // No decay time or zero dt → snap to target immediately.
        mCurrent = mConfig.targetValue;
        return mCurrent;
    }
    const float alpha = 1.0f - fl::expf(-dt / mConfig.decayTauSeconds);
    mCurrent = mCurrent + (mConfig.targetValue - mCurrent) * alpha;
    return mCurrent;
}

void SilenceEnvelope::reset(float initialValue) FL_NOEXCEPT {
    mCurrent = initialValue;
}

bool SilenceEnvelope::isGated(float epsilon) const FL_NOEXCEPT {
    float delta = mCurrent - mConfig.targetValue;
    if (delta < 0.0f) delta = -delta;
    return delta <= epsilon;
}

} // namespace audio
} // namespace fl
