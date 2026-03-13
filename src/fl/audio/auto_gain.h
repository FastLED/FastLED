#pragma once

#include "fl/filter.h"
#include "fl/stl/int.h"
#include "fl/stl/vector.h"

namespace fl {

/// AGC preset selection — derived from WLED Sound Reactive's proven approach.
/// Normal/Vivid/Lazy control how quickly the AGC adapts to source-level changes.
enum AGCPreset {
    AGCPreset_Normal, ///< Balanced: 3.3s peak decay, moderate PI gains
    AGCPreset_Vivid,  ///< Faster response: 1.3s peak decay, higher PI gains
    AGCPreset_Lazy,   ///< Slower, more stable: 6.7s peak decay, lower PI gains
    AGCPreset_Custom  ///< Use custom PI tuning fields below
};

/// Configuration for automatic gain control.
///
/// The AGC uses a PI (proportional-integral) controller with slow peak envelope
/// tracking, inspired by WLED Sound Reactive. It adapts to source-level
/// differences (MEMS mic vs line-in) rather than musical dynamics, avoiding
/// cross-band coupling that plagues fast-tracking AGC designs.
struct AutoGainConfig {
    /// Enable automatic gain adjustment
    bool enabled = true;

    /// Minimum gain multiplier (prevents over-attenuation)
    float minGain = 1.0f / 64.0f;

    /// Maximum gain multiplier (prevents over-amplification)
    float maxGain = 32.0f;

    /// Target RMS level after gain (0-32767)
    /// The AGC will adjust gain to maintain this average level
    float targetRMSLevel = 8000.0f;

    /// AGC behavior preset (default: Normal)
    AGCPreset preset = AGCPreset_Normal;

    // --- Custom PI tuning (only used when preset == AGCPreset_Custom) ---

    /// Peak envelope decay time constant (seconds). Controls how quickly the
    /// peak tracker forgets old peaks. Longer = more stable.
    float peakDecayTau = 3.3f;

    /// Proportional gain for PI controller
    float kp = 0.6f;

    /// Integral gain for PI controller
    float ki = 1.7f;

    /// Slow gain-follow time constant (seconds) — used when error is small
    float gainFollowSlowTau = 12.3f;

    /// Fast gain-follow time constant (seconds) — used when error is large
    float gainFollowFastTau = 0.38f;
};

/// AutoGain implements adaptive gain control using a PI (proportional-integral)
/// controller with slow peak envelope tracking, inspired by WLED Sound Reactive.
///
/// The algorithm:
/// 1. Track peak envelope of input RMS (fast attack ~10ms, slow decay ~3-7s)
/// 2. Compute target gain = targetRMSLevel / peakEnvelope
/// 3. PI controller smoothly drives actual gain toward target gain
/// 4. Large errors use fast time constant, small errors use slow time constant
/// 5. Integrator has anti-windup clamping
///
/// This design adapts to source-level differences (mic sensitivity, line-in
/// voltage) without tracking musical dynamics, preventing cross-band coupling.
///
/// Usage:
/// @code
/// AutoGain agc;
/// AutoGainConfig config;
/// config.preset = AGCPreset_Vivid;  // Faster response
/// agc.configure(config);
///
/// AudioSample sample = ...;
/// AudioSample amplified = agc.process(sample);
/// @endcode
class AutoGain {
public:
    AutoGain();
    explicit AutoGain(const AutoGainConfig& config);
    ~AutoGain();

    /// Configure the auto gain controller
    void configure(const AutoGainConfig& config);

    /// Process audio sample with automatic gain adjustment
    /// @param sample Input audio sample
    /// @return Gain-adjusted audio sample
    AudioSample process(const AudioSample& sample);

    /// Reset internal state
    void reset();

    /// Get current statistics (for monitoring/debugging)
    struct Stats {
        float currentGain = 1.0f;           // Current gain multiplier
        float peakEnvelope = 0.0f;          // Current peak envelope estimate
        float targetGain = 1.0f;            // Target gain from PI controller
        float integrator = 0.0f;            // PI integrator state
        float inputRMS = 0.0f;              // Most recent input RMS
        float outputRMS = 0.0f;             // Most recent output RMS
        u32 samplesProcessed = 0;           // Total samples processed
    };

    const Stats& getStats() const { return mStats; }

    /// Get current gain multiplier
    float getGain() const { return mStats.currentGain; }

    /// Set sample rate for dt computation
    void setSampleRate(int sampleRate) { mSampleRate = sampleRate; }

private:
    /// Resolve preset enum into concrete PI tuning parameters
    void resolvePreset();

    /// Compute target gain from peak envelope
    float computeTargetGain();

    /// Update PI controller toward target gain
    /// @param targetGain Desired gain
    /// @param dt Time step in seconds
    /// @return Smoothed gain output
    float updatePIController(float targetGain, float dt);

    /// Apply gain to audio samples
    /// @param input Input PCM samples
    /// @param gain Gain multiplier
    /// @param output Output buffer
    void applyGain(const vector<i16>& input, float gain, vector<i16>& output);

    AutoGainConfig mConfig;
    Stats mStats;
    int mSampleRate = 44100;

    // Resolved preset parameters (populated by resolvePreset())
    float mPeakDecayTau = 3.3f;
    float mKp = 0.6f;
    float mKi = 1.7f;
    float mGainFollowSlowTau = 12.3f;
    float mGainFollowFastTau = 0.38f;

    /// Peak envelope tracker: fast attack (10ms), slow decay (preset-dependent)
    AttackDecayFilter<float> mPeakEnvelope{0.01f, 3.3f, 0.0f};

    /// PI integrator state
    float mIntegrator = 0.0f;

    /// Last smoothed gain output
    float mLastGain = 1.0f;

    /// Working buffer (reused to avoid allocations)
    vector<i16> mOutputBuffer;
};

} // namespace fl
