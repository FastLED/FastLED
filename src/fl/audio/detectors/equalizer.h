#pragma once

#include "fl/audio/audio_detector.h"
#include "fl/audio/spectral_equalizer.h"
#include "fl/filter.h"
#include "fl/stl/function.h"
#include "fl/stl/span.h"
#include "fl/stl/vector.h"

namespace fl {

/// FFT bin scaling mode applied before per-bin normalization.
/// WLED-MM default is SquareRoot; FastLED previously used None (raw CQT magnitudes).
enum class FFTScalingMode : u8 {
    None,         ///< Raw magnitudes (no scaling)
    SquareRoot,   ///< Square root of magnitude (WLED-MM default, good perceptual balance)
    Logarithmic,  ///< Log10(1 + magnitude) — compresses high peaks
    Linear        ///< Identity (same as None, explicit name for WLED compat)
};

/// Configuration for the equalizer detector.
/// All fields have sensible defaults matching the original hardcoded values.
struct EqualizerConfig {
    float minFreq = 60.0f;           ///< Low end of spectrum (Hz)
    float maxFreq = 5120.0f;         ///< High end of spectrum (Hz)
    float smoothing = 0.05f;         ///< Bin temporal smoothing (ExponentialSmoother tau, used as attack)
    float normAttack = 0.001f;       ///< Normalization attack time (seconds)
    float normDecay = 4.0f;          ///< Normalization decay time (seconds)
    float silenceThreshold = 10.0f;  ///< Raw RMS threshold for silence detection

    /// Output smoothing with separate attack/decay (dynamics limiter).
    /// When outputAttack > 0 AND outputDecay > 0, uses AttackDecayFilter
    /// instead of ExponentialSmoother for per-bin output smoothing.
    /// WLED-MM uses attack ~24-50ms, decay ~250-300ms.
    float outputAttack = 0.0f;       ///< Output attack time (seconds). 0 = use smoothing tau.
    float outputDecay = 0.0f;        ///< Output decay time (seconds). 0 = use smoothing tau.

    /// FFT bin scaling mode applied to raw magnitudes before normalization.
    FFTScalingMode scalingMode = FFTScalingMode::None;

    /// Spectral equalization curve applied to FFT bins before normalization.
    EqualizationCurve curve = EqualizationCurve::Flat;
};

/// Snapshot of equalizer state, passed to onEqualizer callbacks.
struct Equalizer {
    static constexpr int kNumBins = 16;
    float bass = 0;                          ///< 0.0-1.0
    float mid = 0;                           ///< 0.0-1.0
    float treble = 0;                        ///< 0.0-1.0
    float volume = 0;                        ///< 0.0-1.0 (self-normalized RMS)
    float zcf = 0;                           ///< 0.0-1.0 (zero-crossing factor)
    float volumeNormFactor = 1.0f;            ///< Volume normalization factor (1/volumeMax). Higher = quieter input was scaled more.
    bool isSilence = false;                  ///< True when input signal is effectively silent
    span<const float, kNumBins> bins;        ///< 16 bins, each 0.0-1.0

    // P2 features: peak detection and dB mapping
    float dominantFreqHz = 0.0f;             ///< Frequency of strongest bin (Hz)
    float dominantMagnitude = 0.0f;          ///< Magnitude of strongest bin (0.0-1.0, normalized)
    float volumeDb = -100.0f;               ///< Volume in approximate dB (roughly -100 to 0)
};

/// WLED-style equalizer detector that provides a 16-bin frequency spectrum
/// normalized to 0.0-1.0, plus convenience bass/mid/treble/volume getters.
///
/// Unlike FrequencyBands (which gives raw float band energies), this detector
/// outputs pre-normalized values suitable for direct LED mapping.
///
/// Usage:
/// @code
/// // Via AudioProcessor (recommended):
/// audio->onEqualizer([](const Equalizer& eq) {
///     float bass = eq.bass;        // 0.0-1.0
///     float bin5 = eq.bins[5];     // 0.0-1.0
/// });
///
/// // Or polling:
/// float bass = audio.getEqBass();  // 0.0-1.0
/// @endcode
class EqualizerDetector : public AudioDetector {
public:
    static constexpr int kNumBins = 16;

    EqualizerDetector();
    ~EqualizerDetector() override;

    /// Reconfigure equalizer tuning parameters at runtime.
    void configure(const EqualizerConfig& config);

    // AudioDetector interface
    void update(shared_ptr<AudioContext> context) override;
    void fireCallbacks() override;
    bool needsFFT() const override { return true; }
    const char* getName() const override { return "EqualizerDetector"; }
    void reset() override;
    void setSampleRate(int rate) override { mSampleRate = rate; }

    // WLED-compatible getters (all return 0.0-1.0)
    float getBass() const { return mBass; }
    float getMid() const { return mMid; }
    float getTreble() const { return mTreble; }
    float getVolume() const { return mVolume; }
    float getZcf() const { return mZcf; }
    float getVolumeNormFactor() const { return mVolumeNormFactor; }
    /// @deprecated Use getVolumeNormFactor() instead
    float getAutoGain() const { return mVolumeNormFactor; }
    bool getIsSilence() const { return mIsSilence; }
    float getBin(int index) const;
    const float* getBins() const { return mBins; }

    /// Set microphone correction profile (propagated from AudioProcessor).
    void setMicProfile(MicProfile profile);

    // Callback — single struct with everything
    function_list<void(const Equalizer&)> onEqualizer;

    // P2 getters
    float getDominantFreqHz() const { return mDominantFreqHz; }
    float getDominantMagnitude() const { return mDominantMagnitude; }
    float getVolumeDb() const { return mVolumeDb; }

private:
    EqualizerConfig mConfig;
    int mSampleRate = 44100;
    float mBins[kNumBins] = {};
    float mBass = 0, mMid = 0, mTreble = 0, mVolume = 0, mZcf = 0;
    float mVolumeNormFactor = 1.0f;
    bool mIsSilence = false;
    float mGain = 1.0f;
    float mDominantFreqHz = 0.0f;
    float mDominantMagnitude = 0.0f;
    float mVolumeDb = -100.0f;

    // Per-bin adaptive normalization (running max with slow decay)
    vector<AttackDecayFilter<float>> mBinMaxFilters;
    AttackDecayFilter<float> mVolumeMax{0.001f, 4.0f, 0.0f};

    // Smoothing for output stability — may be ExponentialSmoother or AttackDecayFilter
    // depending on whether outputAttack/outputDecay are set
    vector<ExponentialSmoother<float>> mBinSmoothers;
    vector<AttackDecayFilter<float>> mBinOutputFilters;
    bool mUseAttackDecaySmoothing = false;

    // Spectral equalization (optional, applied to raw bins before normalization)
    SpectralEqualizer mSpectralEq;
    float mEqBuffer[kNumBins] = {};

    // Microphone correction gains (applied before spectral EQ)
    float mMicGains[kNumBins] = {};
    bool mHasMicCorrection = false;
    MicProfile mCurrentMicProfile = MicProfile::None;

    // Compute log-spaced bin center frequencies from current config
    void computeBinCenters(float* out) const;

    // Cached FFT from AudioContext (shared across detectors)
    shared_ptr<const FFTBins> mRetainedFFT;

    // FFT windowing correction factor (WLED-MM FFT_DOWNSCALE = 0.40)
    static constexpr float kFFTDownscale = 0.40f;
};

} // namespace fl
