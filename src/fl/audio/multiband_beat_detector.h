#pragma once

#include "fl/int.h"
#include "fl/stl/array.h"
#include "fl/stl/span.h"

namespace fl {

/// Configuration for multi-band beat detection
struct MultiBandBeatDetectorConfig {
    /// Bass beat threshold (0.0-1.0)
    /// Energy increase required to trigger bass beat
    float bassThreshold = 0.15f;

    /// Mid beat threshold (0.0-1.0)
    /// Energy increase required to trigger mid beat
    float midThreshold = 0.12f;

    /// Treble beat threshold (0.0-1.0)
    /// Energy increase required to trigger treble beat
    float trebleThreshold = 0.08f;

    /// Minimum cooldown between beats in same band (frames)
    /// Prevents double-triggering on same beat
    u32 beatCooldownFrames = 10;

    /// Enable cross-band correlation
    /// Boosts confidence when multiple bands trigger simultaneously
    bool enableCrossBandCorrelation = true;

    /// Cross-band correlation boost (0.0-1.0)
    /// Added to threshold when multiple bands trigger together
    float correlationBoost = 0.05f;
};

/// MultiBandBeatDetector performs frequency-specific beat detection.
///
/// This class separates beat detection into three frequency bands:
/// - Bass (bins 0-1): 20-80 Hz - kick drums, bass guitar
/// - Mid (bins 6-7): 320-640 Hz - snares, vocals, guitars
/// - Treble (bins 14-15): 5120-16000 Hz - hi-hats, cymbals
///
/// Features:
/// 1. Per-band energy tracking and threshold adaptation
/// 2. Independent beat detection for each frequency range
/// 3. Cross-band beat correlation (e.g., kick+snare = strong beat)
/// 4. Per-band cooldown to prevent double-triggering
///
/// Usage:
/// @code
/// MultiBandBeatDetector detector;
/// MultiBandBeatDetectorConfig config;
/// config.bassThreshold = 0.15f;
/// config.midThreshold = 0.12f;
/// config.trebleThreshold = 0.08f;
/// detector.configure(config);
///
/// // Each audio frame:
/// float frequencyBins[16] = {...};  // From FrequencyBinMapper
/// detector.detectBeats(frequencyBins);
///
/// if (detector.isBassBeat()) {
///     // Trigger bass-synchronized effect (e.g., strobe on kick)
/// }
/// if (detector.isMidBeat()) {
///     // Trigger mid-synchronized effect (e.g., pulse on snare)
/// }
/// if (detector.isTrebleBeat()) {
///     // Trigger treble-synchronized effect (e.g., sparkle on hi-hat)
/// }
/// @endcode
class MultiBandBeatDetector {
public:
    MultiBandBeatDetector();
    explicit MultiBandBeatDetector(const MultiBandBeatDetectorConfig& config);
    ~MultiBandBeatDetector();

    /// Configure the multi-band beat detector
    void configure(const MultiBandBeatDetectorConfig& config);

    /// Detect beats in all frequency bands
    /// @param frequencyBins 16-element array of frequency bin magnitudes
    void detectBeats(span<const float> frequencyBins);

    /// Check if a bass beat was detected in the last frame
    /// @return True if bass energy increased beyond threshold
    bool isBassBeat() const;

    /// Check if a mid beat was detected in the last frame
    /// @return True if mid energy increased beyond threshold
    bool isMidBeat() const;

    /// Check if a treble beat was detected in the last frame
    /// @return True if treble energy increased beyond threshold
    bool isTrebleBeat() const;

    /// Get current bass energy (0.0-1.0)
    float getBassEnergy() const;

    /// Get current mid energy (0.0-1.0)
    float getMidEnergy() const;

    /// Get current treble energy (0.0-1.0)
    float getTrebleEnergy() const;

    /// Check if multiple bands triggered simultaneously (strong beat)
    /// @return True if 2+ bands detected beats
    bool isMultiBandBeat() const;

    /// Reset internal state (clear history, reset cooldowns)
    void reset();

    /// Get statistics (for debugging/monitoring)
    struct Stats {
        u32 bassBeats = 0;          // Total bass beats detected (lifetime)
        u32 midBeats = 0;           // Total mid beats detected (lifetime)
        u32 trebleBeats = 0;        // Total treble beats detected (lifetime)
        u32 multiBandBeats = 0;     // Beats with multiple bands (lifetime)
        float bassEnergy = 0.0f;    // Current bass energy
        float midEnergy = 0.0f;     // Current mid energy
        float trebleEnergy = 0.0f;  // Current treble energy
    };

    const Stats& getStats() const { return mStats; }

private:
    /// Detect beat in a specific frequency band
    /// @param currentEnergy Current band energy
    /// @param previousEnergy Previous band energy
    /// @param threshold Energy increase threshold
    /// @param cooldownCounter Frame counter for beat cooldown
    /// @return True if beat detected
    bool detectBandBeat(float currentEnergy, float previousEnergy,
                       float threshold, u32& cooldownCounter);

    /// Calculate energy for bass band (bins 0-1)
    /// @param frequencyBins 16-element array of frequency bins
    /// @return Average energy in bass range
    float calculateBassEnergy(span<const float> frequencyBins) const;

    /// Calculate energy for mid band (bins 6-7)
    /// @param frequencyBins 16-element array of frequency bins
    /// @return Average energy in mid range
    float calculateMidEnergy(span<const float> frequencyBins) const;

    /// Calculate energy for treble band (bins 14-15)
    /// @param frequencyBins 16-element array of frequency bins
    /// @return Average energy in treble range
    float calculateTrebleEnergy(span<const float> frequencyBins) const;

    MultiBandBeatDetectorConfig mConfig;
    Stats mStats;

    /// Beat detection flags for current frame
    bool mBassBeat = false;
    bool mMidBeat = false;
    bool mTrebleBeat = false;

    /// Current band energies
    float mBassEnergy = 0.0f;
    float mMidEnergy = 0.0f;
    float mTrebleEnergy = 0.0f;

    /// Previous band energies (for energy increase calculation)
    float mPreviousBassEnergy = 0.0f;
    float mPreviousMidEnergy = 0.0f;
    float mPreviousTrebleEnergy = 0.0f;

    /// Beat cooldown counters (prevent double-triggering)
    u32 mBassCooldown = 0;
    u32 mMidCooldown = 0;
    u32 mTrebleCooldown = 0;

    /// Current frame counter
    u32 mCurrentFrame = 0;

    /// Frequency bin indices for each band
    static constexpr size BASS_BIN_START = 0;
    static constexpr size BASS_BIN_END = 2;    // Bins 0-1
    static constexpr size MID_BIN_START = 6;
    static constexpr size MID_BIN_END = 8;     // Bins 6-7
    static constexpr size TREBLE_BIN_START = 14;
    static constexpr size TREBLE_BIN_END = 16; // Bins 14-15
};

} // namespace fl
