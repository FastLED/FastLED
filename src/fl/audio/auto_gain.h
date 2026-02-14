#pragma once

#include "fl/audio.h"
#include "fl/int.h"
#include "fl/stl/vector.h"

namespace fl {

/// Configuration for automatic gain control
struct AutoGainConfig {
    /// Enable automatic gain adjustment
    bool enabled = true;

    /// Target percentile for ceiling tracking (0.0-1.0)
    /// Default: 0.9 (P90 - track 90th percentile)
    float targetPercentile = 0.9f;

    /// Learning rate for Robbins-Monro percentile estimation
    /// Higher = faster adaptation to level changes
    /// Lower = more stable but slower to adapt
    /// Range: 0.0-1.0, typical: 0.01-0.1
    float learningRate = 0.05f;

    /// Minimum gain multiplier (prevents over-attenuation)
    float minGain = 0.1f;

    /// Maximum gain multiplier (prevents over-amplification)
    float maxGain = 10.0f;

    /// Target RMS level after gain (0-32767)
    /// The AGC will adjust gain to maintain this average level
    float targetRMSLevel = 8000.0f;

    /// Smoothing factor for gain changes (0.0-1.0)
    /// Higher = smoother but slower gain adjustments
    float gainSmoothing = 0.95f;
};

/// AutoGain implements adaptive gain control using Robbins-Monro percentile
/// estimation to track the P90 ceiling (or other configurable percentile).
///
/// The algorithm continuously estimates the target percentile of the signal
/// distribution without storing history, making it memory-efficient and
/// suitable for real-time streaming applications.
///
/// How it works:
/// 1. For each incoming sample, compare RMS to current percentile estimate
/// 2. If RMS > estimate, the estimate was too low → increase it
/// 3. If RMS < estimate, the estimate was too high → decrease it
/// 4. The learning rate controls how quickly the estimate adapts
/// 5. Gain is calculated to bring the percentile estimate to target RMS level
///
/// Usage:
/// @code
/// AutoGain agc;
/// AutoGainConfig config;
/// config.targetPercentile = 0.9f;  // Track P90
/// config.learningRate = 0.05f;
/// config.targetRMSLevel = 8000.0f;
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

    /// Reset internal state (percentile estimate, gain)
    void reset();

    /// Get current statistics (for monitoring/debugging)
    struct Stats {
        float currentGain = 1.0f;           // Current gain multiplier
        float percentileEstimate = 0.0f;    // Current percentile estimate (RMS)
        float inputRMS = 0.0f;              // Most recent input RMS
        float outputRMS = 0.0f;             // Most recent output RMS
        u32 samplesProcessed = 0;           // Total samples processed
    };

    const Stats& getStats() const { return mStats; }

    /// Get current gain multiplier
    float getGain() const { return mStats.currentGain; }

private:
    /// Update percentile estimate using Robbins-Monro algorithm
    /// @param observedRMS Current RMS value
    void updatePercentileEstimate(float observedRMS);

    /// Calculate gain multiplier from percentile estimate
    /// @return Gain multiplier to apply
    float calculateGain();

    /// Apply gain to audio samples
    /// @param input Input PCM samples
    /// @param gain Gain multiplier
    /// @param output Output buffer
    void applyGain(const vector<i16>& input, float gain, vector<i16>& output);

    AutoGainConfig mConfig;
    Stats mStats;

    /// Robbins-Monro percentile estimate (running estimate of target percentile RMS)
    float mPercentileEstimate = 1000.0f;  // Initial estimate

    /// Smoothed gain (to prevent abrupt changes)
    float mSmoothedGain = 1.0f;

    /// Working buffer (reused to avoid allocations)
    vector<i16> mOutputBuffer;
};

} // namespace fl
