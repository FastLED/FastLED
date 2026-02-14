#pragma once

#include "fl/audio.h"
#include "fl/int.h"

namespace fl {

/// Configuration for noise floor tracking
struct NoiseFloorTrackerConfig {
    /// Enable noise floor tracking
    bool enabled = true;

    /// Decay rate for noise floor (0.0-1.0)
    /// Higher = slower decay (more stable, less responsive)
    /// Lower = faster decay (more responsive, less stable)
    float decayRate = 0.99f;

    /// Attack rate for noise floor (0.0-1.0)
    /// How quickly the floor rises when signal is consistently low
    float attackRate = 0.001f;

    /// Hysteresis margin (prevents noise chasing)
    /// Floor must drop by this amount before it can rise again
    float hysteresisMargin = 100.0f;

    /// Minimum floor value (prevents floor from going to zero)
    float minFloor = 10.0f;

    /// Maximum floor value (prevents floor from growing unbounded)
    float maxFloor = 5000.0f;

    /// Cross-domain calibration weight (0.0-1.0)
    /// Blends time-domain RMS and frequency-domain energy for floor estimation
    /// 0.0 = use only time-domain, 1.0 = use only frequency-domain
    float crossDomainWeight = 0.3f;
};

/// NoiseFloorTracker maintains an adaptive estimate of the background noise
/// floor for audio signals, with hysteresis to prevent "noise chasing" where
/// the floor continuously adjusts to the signal level.
///
/// Features:
/// - Exponential decay: Floor gradually decreases toward minimum
/// - Slow attack: Floor increases slowly when signal is consistently low
/// - Hysteresis: Prevents rapid floor oscillations
/// - Cross-domain calibration: Combines time-domain (RMS) and frequency-domain metrics
///
/// How it works:
/// 1. Track minimum observed signal level over time
/// 2. Apply exponential decay to gradually lower the floor
/// 3. If signal stays below floor + margin for sustained period, raise floor slowly
/// 4. Use hysteresis to prevent the floor from chasing transient noise
///
/// Usage:
/// @code
/// NoiseFloorTracker tracker;
/// NoiseFloorTrackerConfig config;
/// config.decayRate = 0.99f;
/// config.hysteresisMargin = 100.0f;
/// tracker.configure(config);
///
/// AudioSample sample = ...;
/// float rms = sample.rms();
/// tracker.update(rms);
///
/// float normalized = tracker.normalize(rms);  // Signal with floor removed
/// bool isAboveFloor = tracker.isAboveFloor(rms);
/// @endcode
class NoiseFloorTracker {
public:
    NoiseFloorTracker();
    explicit NoiseFloorTracker(const NoiseFloorTrackerConfig& config);
    ~NoiseFloorTracker();

    /// Configure the noise floor tracker
    void configure(const NoiseFloorTrackerConfig& config);

    /// Update noise floor estimate with new observation
    /// @param timedomainLevel Time-domain metric (e.g., RMS, peak)
    /// @param frequencydomainLevel Optional frequency-domain metric (e.g., spectral energy)
    void update(float timedomainLevel, float frequencydomainLevel = -1.0f);

    /// Get current noise floor estimate
    float getFloor() const { return mCurrentFloor; }

    /// Normalize signal by removing noise floor
    /// @param level Signal level to normalize
    /// @return Normalized level (clamped to >= 0)
    float normalize(float level) const;

    /// Check if signal is above noise floor + margin
    /// @param level Signal level to check
    /// @return True if signal exceeds floor + hysteresis margin
    bool isAboveFloor(float level) const;

    /// Reset noise floor to initial state
    void reset();

    /// Get current statistics (for monitoring/debugging)
    struct Stats {
        float currentFloor = 0.0f;        // Current noise floor estimate
        float minObserved = 0.0f;         // Minimum level observed (lifetime)
        float maxObserved = 0.0f;         // Maximum level observed (lifetime)
        u32 samplesProcessed = 0;         // Total samples processed
        bool inHysteresis = false;        // Whether hysteresis is active
    };

    const Stats& getStats() const { return mStats; }

private:
    /// Update floor estimate based on current observation
    /// @param level Observed signal level
    void updateFloor(float level);

    /// Calculate combined metric from time and frequency domains
    /// @param timeLevel Time-domain level
    /// @param freqLevel Frequency-domain level
    /// @return Weighted combination
    float combineDomains(float timeLevel, float freqLevel) const;

    NoiseFloorTrackerConfig mConfig;
    Stats mStats;

    /// Current noise floor estimate
    float mCurrentFloor = 100.0f;

    /// Floor value at last hysteresis trigger
    /// Used to enforce hysteresis margin before allowing floor to rise
    float mLastHysteresisFloor = 0.0f;

    /// Count of consecutive samples below floor (for slow attack)
    u32 mBelowFloorCount = 0;

    /// Threshold for considering signal "consistently low"
    static constexpr u32 BELOW_FLOOR_THRESHOLD = 10;
};

} // namespace fl
