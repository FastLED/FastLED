#pragma once


#if 0 // EXPLICITLY DISABLED

#include "fl/algorithm.h"
#include "fl/math.h"
#include "fl/stdint.h"
#include "fl/math_macros.h"

namespace fl {

/// Tracks a smoothed peak with attack, decay, and output-inertia time-constants.
/// This is useful for creating smooth audio reactive visualizations that respond
/// to audio input with configurable rise and fall characteristics.
class MaxFadeTracker {
public:
    /// @param attackTimeSec  τ₁: how quickly to rise toward a new peak (seconds).
    /// @param decayTimeSec   τ₂: how quickly to decay to 1/e of value (seconds).
    /// @param outputTimeSec  τ₃: how quickly the returned value follows currentLevel_ (seconds).
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

    /// Update attack time constant
    void setAttackTime(float t) { attackRate_ = 1.0f / t; }
    
    /// Update decay time constant
    void setDecayTime(float t) { decayRate_ = 1.0f / t; }
    
    /// Update output smoothing time constant
    void setOutputTime(float t) { outputRate_ = 1.0f / t; }

    /// Process a block of audio samples and return smoothed peak level [0…1].
    /// @param samples pointer to int16_t audio samples
    /// @param length number of samples in the block
    /// @returns smoothed peak level with attack/decay/output inertia applied
    float operator()(const int16_t* samples, size_t length) {
        // 1) Find peak value in this block
        float peak = 0.0f;
        for (size_t i = 0; i < length; ++i) {
            float v = ABS(samples[i]) * (1.0f / 32768.0f);
            peak = MAX(peak, v);
        }

        // 2) Calculate time delta for this block
        float dt = static_cast<float>(length) / sampleRate_;

        // 3) Update currentLevel_ with attack/decay behavior
        if (peak > currentLevel_) {
            // Attack: rise toward new peak
            float riseFactor = 1.0f - std::exp(-attackRate_ * dt);
            currentLevel_ += (peak - currentLevel_) * riseFactor;
        } else {
            // Decay: exponential decay
            float decayFactor = std::exp(-decayRate_ * dt);
            currentLevel_ *= decayFactor;
        }

        // 4) Apply output smoothing/inertia
        float outFactor = 1.0f - std::exp(-outputRate_ * dt);
        smoothedOutput_ += (currentLevel_ - smoothedOutput_) * outFactor;

        return smoothedOutput_;
    }

    /// Get current peak level without processing new samples
    float getCurrentLevel() const { return currentLevel_; }
    
    /// Get current smoothed output level
    float getSmoothedOutput() const { return smoothedOutput_; }

    /// Reset tracker to initial state
    void reset() {
        currentLevel_ = 0.0f;
        smoothedOutput_ = 0.0f;
    }

private:
    float attackRate_;     // = 1/τ₁ (attack time constant)
    float decayRate_;      // = 1/τ₂ (decay time constant)  
    float outputRate_;     // = 1/τ₃ (output smoothing time constant)
    float sampleRate_;     // audio sample rate
    float currentLevel_;   // instantaneous peak with attack/decay applied
    float smoothedOutput_; // final output value with inertia
};

} // namespace fl

#endif
