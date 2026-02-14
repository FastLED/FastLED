#pragma once

#include "fl/int.h"
#include "fl/stl/vector.h"
#include "fl/stl/span.h"
#include "fl/stl/array.h"

namespace fl {

/// Equalization curve type
enum class EqualizationCurve {
    Flat,        // No equalization (all gains = 1.0)
    AWeighting,  // A-weighting curve (emphasizes 1-6 kHz, de-emphasizes bass/treble)
    Custom       // User-defined per-band gains
};

/// Configuration for spectral equalizer
struct SpectralEqualizerConfig {
    /// Equalization curve type
    EqualizationCurve curve = EqualizationCurve::Flat;

    /// Number of frequency bands (must match FrequencyBinMapper output)
    size numBands = 16;

    /// Custom per-band gain multipliers (only used if curve = Custom)
    /// Size must match numBands. Default: all 1.0 (no eq)
    vector<float> customGains;

    /// Apply makeup gain to compensate for overall level changes
    /// If true, automatically adjusts overall gain to maintain average level
    bool applyMakeupGain = false;

    /// Makeup gain target level (0.0-1.0)
    /// The equalizer will scale output to maintain this average level
    float makeupGainTarget = 0.5f;

    /// Enable dynamic range compression per band
    /// Compresses loud signals to reduce dynamic range
    bool enableCompression = false;

    /// Compression threshold (0.0-1.0)
    /// Signals above this level are compressed
    float compressionThreshold = 0.7f;

    /// Compression ratio (1.0 = no compression, higher = more compression)
    /// 2.0 = 2:1 ratio, 4.0 = 4:1 ratio, etc.
    float compressionRatio = 2.0f;
};

/// SpectralEqualizer applies frequency-dependent gain correction to address
/// mid-frequency dominance and provide perceptual weighting of audio spectra.
///
/// Common use cases:
/// - A-weighting: Emphasize frequencies where human hearing is most sensitive (1-6 kHz)
/// - Mid-scoop: Reduce mid frequencies to balance bass/treble in visual effects
/// - Custom EQ: User-defined per-band gain adjustments
///
/// The equalizer operates on frequency bins produced by FrequencyBinMapper and
/// applies configurable gain curves to reshape the spectrum before beat detection
/// and visual processing.
///
/// Usage:
/// @code
/// SpectralEqualizer eq;
/// SpectralEqualizerConfig config;
/// config.curve = EqualizationCurve::AWeighting;
/// config.numBands = 16;
/// eq.configure(config);
///
/// // Process frequency bins from FrequencyBinMapper
/// vector<float> frequencyBins = ...;  // From mapper.mapBins()
/// vector<float> equalizedBins(16);
/// eq.apply(frequencyBins, equalizedBins);
/// @endcode
class SpectralEqualizer {
public:
    SpectralEqualizer();
    explicit SpectralEqualizer(const SpectralEqualizerConfig& config);
    ~SpectralEqualizer();

    /// Configure the spectral equalizer
    /// This calculates per-band gain multipliers based on the selected curve
    void configure(const SpectralEqualizerConfig& config);

    /// Apply equalization to frequency bins
    /// @param inputBins Input frequency bins (from FrequencyBinMapper)
    /// @param outputBins Output equalized frequency bins
    void apply(span<const float> inputBins, span<float> outputBins) const;

    /// Set custom per-band gains (switches to Custom curve)
    /// @param gains Per-band gain multipliers (size must match numBands)
    void setCustomGains(span<const float> gains);

    /// Get current per-band gains
    /// @return Span of gain multipliers for each band
    span<const float> getGains() const { return mGains; }

    /// Get current configuration
    const SpectralEqualizerConfig& getConfig() const { return mConfig; }

    /// Get statistics (for debugging/monitoring)
    struct Stats {
        u32 applicationsCount = 0;      // Total applications performed
        float lastInputPeak = 0.0f;     // Peak value in last input
        float lastOutputPeak = 0.0f;    // Peak value in last output
        float lastMakeupGain = 1.0f;    // Makeup gain applied in last call
        float avgInputLevel = 0.0f;     // Average input level (last call)
        float avgOutputLevel = 0.0f;    // Average output level (last call)
    };

    const Stats& getStats() const { return mStats; }

    /// Reset statistics
    void resetStats();

private:
    /// Calculate gains based on current curve
    void calculateGains();

    /// Calculate A-weighting gains
    void calculateAWeightingGains();

    /// Calculate flat gains (all 1.0)
    void calculateFlatGains();

    /// Calculate makeup gain to maintain target level
    /// @param inputBins Input frequency bins
    /// @param outputBins Output frequency bins (after EQ)
    /// @return Makeup gain multiplier
    float calculateMakeupGain(span<const float> inputBins, span<const float> outputBins) const;

    /// Apply dynamic range compression per band
    /// @param value Input value
    /// @return Compressed value
    float applyCompression(float value) const;

    SpectralEqualizerConfig mConfig;
    Stats mStats;

    /// Per-band gain multipliers
    vector<float> mGains;

    /// A-weighting coefficients for 16-band frequency analysis
    /// These approximate the A-weighting curve across logarithmic frequency bins
    /// Values emphasize 1-6 kHz range where human hearing is most sensitive
    static constexpr float A_WEIGHTING_16BAND[16] = {
        0.5f,  // Bin 0: 20-40 Hz (bass rolloff)
        0.6f,  // Bin 1: 40-80 Hz (bass rolloff)
        0.8f,  // Bin 2: 80-160 Hz (gradual increase)
        1.0f,  // Bin 3: 160-320 Hz (flat)
        1.2f,  // Bin 4: 320-640 Hz (emphasis begins)
        1.3f,  // Bin 5: 640-1280 Hz (emphasis)
        1.4f,  // Bin 6: 1280-2560 Hz (peak emphasis)
        1.4f,  // Bin 7: 2560-5120 Hz (peak emphasis)
        1.3f,  // Bin 8: 5120-10240 Hz (gradual rolloff)
        1.2f,  // Bin 9: 10240-16000 Hz (rolloff continues)
        1.0f,  // Bin 10 (fallback)
        0.8f,  // Bin 11 (fallback)
        0.6f,  // Bin 12 (fallback)
        0.4f,  // Bin 13 (fallback)
        0.2f,  // Bin 14 (fallback)
        0.1f   // Bin 15 (fallback)
    };

    /// A-weighting coefficients for 32-band frequency analysis
    static constexpr float A_WEIGHTING_32BAND[32] = {
        0.3f, 0.4f, 0.5f, 0.6f, 0.7f, 0.8f, 0.9f, 1.0f,  // Bass rolloff (0-7)
        1.1f, 1.2f, 1.3f, 1.4f, 1.4f, 1.4f, 1.3f, 1.2f,  // Mid emphasis (8-15)
        1.1f, 1.0f, 0.9f, 0.8f, 0.7f, 0.6f, 0.5f, 0.4f,  // Treble rolloff (16-23)
        0.3f, 0.2f, 0.2f, 0.1f, 0.1f, 0.1f, 0.1f, 0.1f   // High freq rolloff (24-31)
    };
};

} // namespace fl
