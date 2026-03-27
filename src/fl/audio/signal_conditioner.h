#pragma once

#include "fl/stl/int.h"
#include "fl/stl/vector.h"
#include "fl/stl/span.h"
#include "fl/stl/noexcept.h"

namespace fl {
namespace audio {

/// Configuration for signal conditioning pipeline
struct SignalConditionerConfig {
    /// Enable DC offset removal (running average high-pass filter)
    bool enableDCRemoval = true;

    /// Enable spike filtering for I2S glitches
    bool enableSpikeFilter = true;

    /// Enable noise gate with hysteresis
    bool enableNoiseGate = true;

    /// Spike detection threshold (absolute value)
    /// Samples beyond ±spikeThreshold are rejected as glitches
    i16 spikeThreshold = 10000;

    /// Noise gate open threshold (signal must exceed to open gate)
    i16 noiseGateOpenThreshold = 500;

    /// Noise gate close threshold (signal must fall below to close gate)
    i16 noiseGateCloseThreshold = 300;

    // DC removal uses per-buffer instantaneous calculation (no alpha needed)
};

/// SignalConditioner performs low-level audio preprocessing to clean
/// raw PCM samples before FFT analysis or beat detection.
///
/// Pipeline stages:
/// 1. Spike filtering - Rejects I2S glitches (samples beyond threshold)
/// 2. DC offset removal - Subtracts running average to center signal at zero
/// 3. Noise gate - Applies hysteresis gating to suppress background noise
///
/// Usage:
/// @code
/// SignalConditioner conditioner;
/// SignalConditionerConfig config;
/// config.spikeThreshold = 10000;
/// config.noiseGateOpenThreshold = 500;
/// conditioner.configure(config);
///
/// Sample rawSample = ...;  // From I2S microphone
/// Sample cleanSample = conditioner.processSample(rawSample);
/// @endcode
class SignalConditioner {
public:
    SignalConditioner() FL_NOEXCEPT;
    explicit SignalConditioner(const SignalConditionerConfig& config);
    ~SignalConditioner() FL_NOEXCEPT;

    /// Configure the signal conditioner
    void configure(const SignalConditionerConfig& config);

    /// Process a raw audio sample through the conditioning pipeline
    /// @param sample Raw audio sample from microphone/I2S
    /// @return Cleaned audio sample (DC-removed, spike-filtered, gated)
    Sample processSample(const Sample& sample);

    /// Reset internal state (DC estimate, noise gate state)
    void reset();

    /// Get current statistics (for debugging/monitoring)
    struct Stats {
        i32 dcOffset = 0;           // Current DC offset estimate
        bool noiseGateOpen = false; // Current noise gate state
        u32 spikesRejected = 0;     // Count of spikes rejected (lifetime)
        u32 samplesProcessed = 0;   // Total samples processed (lifetime)
    };

    const Stats& getStats() const { return mStats; }

private:
    /// Detect and reject spike samples
    /// @param pcm Input PCM samples
    /// @param validMask Output mask (true = valid, false = spike)
    /// @return Count of valid samples
    size filterSpikes(span<const i16> pcm, vector<bool>& validMask);

    /// Calculate DC offset from valid samples only
    /// @param pcm Input PCM samples
    /// @param validMask Validity mask from spike filtering
    /// @return Estimated DC offset
    i32 calculateDCOffset(span<const i16> pcm, const vector<bool>& validMask);

    /// Remove DC offset from samples
    /// @param pcm Input PCM samples
    /// @param dcOffset DC offset to remove
    /// @param output Output buffer for DC-removed samples
    void removeDCOffset(span<const i16> pcm, i32 dcOffset, vector<i16>& output);

    /// Apply noise gate with hysteresis
    /// @param pcm Input PCM samples (DC-removed)
    /// @param output Output buffer (gated samples, zero if gate closed)
    void applyNoiseGate(span<const i16> pcm, vector<i16>& output);

    SignalConditionerConfig mConfig;
    Stats mStats;

    /// Noise gate state
    bool mNoiseGateOpen = false;

    /// Working buffers (reused to avoid allocations)
    vector<bool> mValidMask;
    vector<i16> mTempBuffer;
    vector<i16> mOutputBuffer;
};

} // namespace audio
} // namespace fl
