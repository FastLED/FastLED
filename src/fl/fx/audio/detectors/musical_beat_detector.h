#pragma once

#include "fl/int.h"
#include "fl/stl/vector.h"
#include "fl/stl/span.h"

namespace fl {

/// Configuration for musical beat detection
struct MusicalBeatDetectorConfig {
    /// Minimum BPM to detect (default: 50 BPM)
    float minBPM = 50.0f;

    /// Maximum BPM to detect (default: 250 BPM)
    float maxBPM = 250.0f;

    /// Minimum beat confidence to report a beat (0.0-1.0)
    /// Higher values = fewer false positives, may miss weak beats
    float minBeatConfidence = 0.5f;

    /// BPM estimation smoothing factor (0.0-1.0)
    /// Higher values = slower BPM adaptation, more stable tempo
    float bpmSmoothingAlpha = 0.9f;

    /// Sample rate (Hz) - used for timing calculations
    u32 sampleRate = 22050;

    /// Samples per frame - used for timing calculations
    u32 samplesPerFrame = 512;

    /// Maximum number of inter-beat intervals to track
    /// Higher values = better BPM estimation, more memory
    u32 maxIBIHistory = 8;
};

/// MusicalBeatDetector distinguishes true musical beats from random onset detection.
///
/// This class improves upon basic spectral flux onset detection by:
/// 1. Tracking inter-beat intervals (IBI) to detect rhythmic patterns
/// 2. Estimating BPM and validating beat candidates against the detected tempo
/// 3. Applying confidence scoring based on tempo consistency
/// 4. Rejecting random onsets that don't fit the detected tempo
///
/// Key insight: Basic onset detection triggers on ANY spectral change, not just
/// musical beats. This detector uses temporal pattern recognition to distinguish
/// true beats from random noise bursts or non-rhythmic transients.
///
/// Usage:
/// @code
/// MusicalBeatDetector detector;
/// MusicalBeatDetectorConfig config;
/// config.minBPM = 60.0f;
/// config.maxBPM = 180.0f;
/// detector.configure(config);
///
/// // Each audio frame:
/// bool onsetDetected = ... // From SpectralFluxDetector
/// float onsetStrength = ... // Onset magnitude
/// detector.processSample(onsetDetected, onsetStrength);
///
/// if (detector.isBeat()) {
///     float bpm = detector.getBPM();
///     float confidence = detector.getBeatConfidence();
///     // Trigger beat-synchronized effect
/// }
/// @endcode
class MusicalBeatDetector {
public:
    MusicalBeatDetector();
    explicit MusicalBeatDetector(const MusicalBeatDetectorConfig& config);
    ~MusicalBeatDetector();

    /// Configure the beat detector
    void configure(const MusicalBeatDetectorConfig& config);

    /// Process one audio frame
    /// @param onsetDetected True if onset detector triggered
    /// @param onsetStrength Onset magnitude (spectral flux value)
    void processSample(bool onsetDetected, float onsetStrength);

    /// Check if a musical beat was detected in the last frame
    /// @return True if a beat with sufficient confidence was detected
    bool isBeat() const;

    /// Get current BPM estimate
    /// @return Estimated tempo in beats per minute (50-250 BPM)
    float getBPM() const;

    /// Get beat confidence for the last detected beat
    /// @return Confidence score (0.0-1.0), higher = more rhythmic consistency
    float getBeatConfidence() const;

    /// Get inter-beat interval (IBI) statistics
    /// @return Average IBI in seconds (time between beats)
    float getAverageIBI() const;

    /// Reset internal state (clear history, reset BPM)
    void reset();

    /// Get statistics (for debugging/monitoring)
    struct Stats {
        u32 totalOnsets = 0;        // Total onsets detected (lifetime)
        u32 validatedBeats = 0;     // Onsets validated as beats (lifetime)
        u32 rejectedOnsets = 0;     // Onsets rejected (not rhythmic)
        float currentBPM = 0.0f;    // Current BPM estimate
        float averageIBI = 0.0f;    // Average inter-beat interval (seconds)
        u32 ibiCount = 0;           // Number of IBIs in history
    };

    const Stats& getStats() const { return mStats; }

private:
    /// Validate if an onset is a true musical beat
    /// @param onsetStrength Onset magnitude
    /// @return True if onset matches expected beat timing
    bool validateBeat(float onsetStrength);

    /// Calculate beat confidence based on rhythmic consistency
    /// @param currentIBI Current inter-beat interval
    /// @return Confidence score (0.0-1.0)
    float calculateBeatConfidence(float currentIBI);

    /// Update BPM estimate from inter-beat intervals
    void updateBPMEstimate();

    /// Check if IBI is within valid BPM range
    /// @param ibi Inter-beat interval in seconds
    /// @return True if IBI corresponds to valid BPM (minBPM-maxBPM)
    bool isValidIBI(float ibi) const;

    /// Calculate standard deviation of IBI history
    /// @return Standard deviation in seconds (lower = more consistent tempo)
    float calculateIBIStdDev() const;

    MusicalBeatDetectorConfig mConfig;
    Stats mStats;

    /// Last beat was detected
    bool mBeatDetected = false;

    /// Last beat confidence score
    float mLastBeatConfidence = 0.0f;

    /// Current BPM estimate (smoothed)
    float mCurrentBPM = 120.0f;

    /// Last beat timestamp (in frames)
    u32 mLastBeatFrame = 0;

    /// Current frame counter
    u32 mCurrentFrame = 0;

    /// Inter-beat interval history (in frames)
    vector<u32> mIBIHistory;

    /// Average inter-beat interval (in frames)
    float mAverageIBI = 0.0f;
};

} // namespace fl
