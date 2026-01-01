#pragma once
#include "fl/time_alpha.h"
#include "fl/math_macros.h"

/// Tracks a smoothed peak with attack, decay, and output-inertia time-constants.
class MaxFadeTracker {
public:
    /// @param attackTimeSec  τ₁: how quickly to rise toward a new peak.
    /// @param decayTimeSec   τ₂: how quickly to decay to 1/e of value.
    /// @param outputTimeSec  τ₃: how quickly the returned value follows mCurrentLevel.
    /// @param sampleRate     audio sample rate (e.g. 44100 or 48000).
    MaxFadeTracker(float attackTimeSec,
                   float decayTimeSec,
                   float outputTimeSec,
                   float sampleRate)
      : mAttackRate(1.0f / attackTimeSec)
      , mDecayRate(1.0f / decayTimeSec)
      , mOutputRate(1.0f / outputTimeSec)
      , mSampleRate(sampleRate)
      , mCurrentLevel(0.0f)
      , mSmoothedOutput(0.0f)
    {}

    void setAttackTime(float t){ mAttackRate = 1.0f/t; }
    void setDecayTime (float t){  mDecayRate = 1.0f/t; }
    void setOutputTime(float t){ mOutputRate = 1.0f/t; }

    /// Process one 512-sample block; returns [0…1] with inertia.
    float operator()(const int16_t* samples, size_t length) {
        assert(length == 512);
        // 1) block peak
        float peak = 0.0f;
        for (size_t i = 0; i < length; ++i) {
            float v = FL_ABS(samples[i]) * (1.0f/32768.0f);
            peak = FL_MAX(peak, v);
        }

        // 2) time delta
        float dt = static_cast<float>(length) / mSampleRate;

        // 3) update mCurrentLevel with attack/decay
        if (peak > mCurrentLevel) {
            float riseFactor = 1.0f - fl::exp(-mAttackRate * dt);
            mCurrentLevel += (peak - mCurrentLevel) * riseFactor;
        } else {
            float decayFactor = fl::exp(-mDecayRate * dt);
            mCurrentLevel *= decayFactor;
        }

        // 4) output inertia: smooth mSmoothedOutput → mCurrentLevel
        float outFactor = 1.0f - fl::exp(-mOutputRate * dt);
        mSmoothedOutput += (mCurrentLevel - mSmoothedOutput) * outFactor;

        return mSmoothedOutput;
    }

private:
    float mAttackRate;    // = 1/τ₁
    float mDecayRate;     // = 1/τ₂
    float mOutputRate;    // = 1/τ₃
    float mSampleRate;
    float mCurrentLevel;  // instantaneous peak with attack/decay
    float mSmoothedOutput; // returned value with inertia
};
