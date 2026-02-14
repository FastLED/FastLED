#pragma once

#include "fl/int.h"
#include "fl/stl/vector.h"
#include "fl/stl/span.h"
#include "fl/stl/array.h"

namespace fl {

/// Frequency bin mode - controls number of output bins
enum class FrequencyBinMode {
    Bins16 = 16,  // 16-bin mode (default, matches WLED)
    Bins32 = 32   // 32-bin mode (higher resolution)
};

/// Configuration for frequency bin mapping
struct FrequencyBinMapperConfig {
    /// Number of output frequency bins (16 or 32)
    FrequencyBinMode mode = FrequencyBinMode::Bins16;

    /// Minimum frequency (Hz) - default 20 Hz (bass)
    float minFrequency = 20.0f;

    /// Maximum frequency (Hz) - default 16000 Hz (treble)
    float maxFrequency = 16000.0f;

    /// Sample rate (Hz) - must match FFT sample rate
    u32 sampleRate = 22050;

    /// Number of FFT bins available from FFT output
    /// For 512-sample FFT at 22050 Hz: 256 bins (512/2)
    u32 fftBinCount = 256;

    /// Use logarithmic spacing (recommended for audio)
    /// Logarithmic spacing provides better bass/mid/treble separation
    bool useLogSpacing = true;
};

/// FrequencyBinMapper maps FFT output bins to perceptually-spaced frequency channels.
///
/// This class converts the linear FFT spectrum into logarithmically-spaced frequency
/// bands that better match human hearing. It supports both 16-bin (WLED-compatible)
/// and 32-bin (higher resolution) modes with pre-calculated bin boundaries for
/// efficient real-time processing.
///
/// Frequency ranges (16-bin mode, log spacing 20-16000 Hz):
/// - Bass: bins 0-1 (20-80 Hz)
/// - Mid: bins 6-7 (320-640 Hz)
/// - Treble: bins 14-15 (5120-16000 Hz)
///
/// Usage:
/// @code
/// FrequencyBinMapper mapper;
/// FrequencyBinMapperConfig config;
/// config.mode = FrequencyBinMode::Bins16;
/// config.sampleRate = 22050;
/// config.fftBinCount = 256;  // 512-sample FFT / 2
/// mapper.configure(config);
///
/// // Map FFT output to frequency bins
/// FFTBins fftOutput = ...;  // From FFT
/// vector<float> frequencyBins(16);
/// mapper.mapBins(fftOutput.bins_raw, frequencyBins);
///
/// // Access specific ranges
/// float bassEnergy = mapper.getBassEnergy(frequencyBins);
/// float midEnergy = mapper.getMidEnergy(frequencyBins);
/// float trebleEnergy = mapper.getTrebleEnergy(frequencyBins);
/// @endcode
class FrequencyBinMapper {
public:
    FrequencyBinMapper();
    explicit FrequencyBinMapper(const FrequencyBinMapperConfig& config);
    ~FrequencyBinMapper();

    /// Configure the frequency bin mapper
    /// This calculates bin boundaries and FFT-to-frequency bin mappings
    void configure(const FrequencyBinMapperConfig& config);

    /// Map FFT bins to frequency channels
    /// @param fftBins Input FFT bins (magnitude spectrum)
    /// @param outputBins Output frequency bins (16 or 32 bins depending on mode)
    void mapBins(span<const float> fftBins, span<float> outputBins) const;

    /// Get bass energy (average of bins 0-1 in 16-bin mode)
    /// @param frequencyBins Frequency bins from mapBins()
    /// @return Average bass energy (0.0-1.0 normalized)
    float getBassEnergy(span<const float> frequencyBins) const;

    /// Get mid energy (average of bins 6-7 in 16-bin mode)
    /// @param frequencyBins Frequency bins from mapBins()
    /// @return Average mid energy (0.0-1.0 normalized)
    float getMidEnergy(span<const float> frequencyBins) const;

    /// Get treble energy (average of bins 14-15 in 16-bin mode)
    /// @param frequencyBins Frequency bins from mapBins()
    /// @return Average treble energy (0.0-1.0 normalized)
    float getTrebleEnergy(span<const float> frequencyBins) const;

    /// Get frequency boundaries for a specific output bin
    /// @param binIndex Output bin index (0 to numBins-1)
    /// @return Pair of (minFreq, maxFreq) in Hz
    struct FrequencyRange {
        float minFreq;
        float maxFreq;
    };
    FrequencyRange getBinFrequencyRange(size binIndex) const;

    /// Get current configuration
    const FrequencyBinMapperConfig& getConfig() const { return mConfig; }

    /// Get number of output bins (16 or 32)
    size getNumBins() const { return static_cast<size>(mConfig.mode); }

    /// Get statistics (for debugging/monitoring)
    struct Stats {
        u32 binMappingCount = 0;    // Total bin mappings performed
        u32 lastFFTBinsUsed = 0;    // Number of FFT bins used in last mapping
        float maxMagnitude = 0.0f;  // Maximum magnitude in last mapping
    };

    const Stats& getStats() const { return mStats; }

private:
    /// Calculate frequency bin boundaries (linear or logarithmic spacing)
    void calculateBinBoundaries();

    /// Calculate FFT bin to frequency bin mappings
    /// Pre-calculates which FFT bins contribute to each frequency bin
    void calculateBinMappings();

    /// Convert frequency (Hz) to FFT bin index
    /// @param frequency Frequency in Hz
    /// @return FFT bin index (may be fractional)
    float frequencyToFFTBin(float frequency) const;

    /// Calculate logarithmically-spaced frequency boundaries
    void calculateLogFrequencies();

    /// Calculate linearly-spaced frequency boundaries
    void calculateLinearFrequencies();

    FrequencyBinMapperConfig mConfig;
    Stats mStats;

    /// Pre-calculated frequency boundaries for each output bin
    /// Size: numBins + 1 (includes both lower and upper edges)
    vector<float> mBinFrequencies;

    /// Mapping from output bins to FFT bin ranges
    /// Each entry contains (startBin, endBin) for averaging
    struct BinMapping {
        u32 startBin;  // First FFT bin (inclusive)
        u32 endBin;    // Last FFT bin (exclusive)
    };
    vector<BinMapping> mBinMappings;

    /// Bass/mid/treble bin indices (for 16-bin mode)
    /// These are pre-calculated based on the bin count
    static constexpr size BASS_BIN_START = 0;
    static constexpr size BASS_BIN_END = 2;    // Bins 0-1 (exclusive end)
    static constexpr size MID_BIN_START = 6;
    static constexpr size MID_BIN_END = 8;     // Bins 6-7 (exclusive end)
    static constexpr size TREBLE_BIN_START = 14;
    static constexpr size TREBLE_BIN_END = 16; // Bins 14-15 (exclusive end)
};

} // namespace fl
