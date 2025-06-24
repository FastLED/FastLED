#pragma once
#include "fl/time_alpha.h"
#include "fl/math_macros.h"

/// Tracks a smoothed peak with attack, decay, and output-inertia time-constants.
class MaxFadeTracker {
public:
    /// @param attackTimeSec  τ₁: how quickly to rise toward a new peak.
    /// @param decayTimeSec   τ₂: how quickly to decay to 1/e of value.
    /// @param outputTimeSec  τ₃: how quickly the returned value follows currentLevel_.
    /// @param sampleRate     audio sample rate (e.g. 44100 or 48000).
    MaxFadeTracker(float attackTimeSec,
                   float decayTimeSec,
                   float outputTimeSec,
                   float sampleRate)
      : attackRate_(1.0f / attackTimeSec)
      , decayRate_(1.0f / decayTimeSec)
      , outputRate_(1.0f / outputTimeSec)
      , sampleRate_(sampleRate)
      , currentLevel_(0.0f)
      , smoothedOutput_(0.0f)
    {}

    void setAttackTime(float t){ attackRate_ = 1.0f/t; }
    void setDecayTime (float t){  decayRate_ = 1.0f/t; }
    void setOutputTime(float t){ outputRate_ = 1.0f/t; }

    /// Process one 512-sample block; returns [0…1] with inertia.
    float operator()(const int16_t* samples, size_t length) {
        assert(length == 512);
        // 1) block peak
        float peak = 0.0f;
        for (size_t i = 0; i < length; ++i) {
            float v = ABS(samples[i]) * (1.0f/32768.0f);
            peak = MAX(peak, v);
        }

        // 2) time delta
        float dt = static_cast<float>(length) / sampleRate_;

        // 3) update currentLevel_ with attack/decay
        if (peak > currentLevel_) {
            float riseFactor = 1.0f - fl::exp(-attackRate_ * dt);
            currentLevel_ += (peak - currentLevel_) * riseFactor;
        } else {
            float decayFactor = fl::exp(-decayRate_ * dt);
            currentLevel_ *= decayFactor;
        }

        // 4) output inertia: smooth smoothedOutput_ → currentLevel_
        float outFactor = 1.0f - fl::exp(-outputRate_ * dt);
        smoothedOutput_ += (currentLevel_ - smoothedOutput_) * outFactor;

        return smoothedOutput_;
    }

private:
    float attackRate_;    // = 1/τ₁
    float decayRate_;     // = 1/τ₂
    float outputRate_;    // = 1/τ₃
    float sampleRate_;
    float currentLevel_;  // instantaneous peak with attack/decay
    float smoothedOutput_; // returned value with inertia
};
