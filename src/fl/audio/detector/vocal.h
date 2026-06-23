// Vocal - Human voice detection using spectral characteristics
// Part of FastLED Audio System v2.0 - Phase 3 (Differentiators)
//
// Detects human voice in audio using spectral centroid, spectral rolloff,
// and formant ratio analysis. Provides confidence-based detection with
// hysteresis for stable vocal/non-vocal classification.

#pragma once

#include "fl/audio/audio_detector.h"
#include "fl/math/filter/filter.h"
#include "fl/math/math.h"
#include "fl/stl/function.h"
#include "fl/stl/shared_ptr.h"
#include "fl/stl/noexcept.h"

namespace fl {
namespace audio {
namespace detector {

class Vocal : public Detector {
public:
    Vocal() FL_NOEXCEPT;
    ~Vocal() FL_NOEXCEPT override;

    // Detector interface
    void update(shared_ptr<Context> context) override;
    void fireCallbacks() override;
    bool needsFFT() const override { return true; }
    const char* getName() const override { return "Vocal"; }
    void reset() override;
    void setSampleRate(int sampleRate) override { mSampleRate = sampleRate; }

    // Callbacks (multiple listeners supported)
    function_list<void(u8 active)> onVocal;
    function_list<void()> onVocalStart;
    function_list<void()> onVocalEnd;

    // State access
    bool isVocal() const { return mVocalActive; }
    float getConfidence() const { return mConfidenceSmoother.value(); }
    void setThreshold(float threshold) { mOnThreshold = threshold; mOffThreshold = fl::max(0.0f, threshold - 0.13f); }
    void setSmoothingAlpha(float tau) { mConfidenceSmoother.setTau(tau); }

    friend struct VocalDetectorDiagnostics;

private:
    bool mVocalActive;
    bool mPreviousVocalActive;
    bool mStateChanged = false;
    float mConfidence;
    float mOnThreshold = 0.65f;
    float mOffThreshold = 0.52f;
    // Time-aware confidence smoothing (tau=0.05s ≈ old alpha=0.7 at 43fps)
    ExponentialSmoother<float> mConfidenceSmoother{0.05f};
    int mFramesInState = 0; // Debounce counter
    static constexpr int MIN_FRAMES_TO_TRANSITION = 3; // Debounce: require N frames before state change
    float mSpectralCentroid;
    float mSpectralRolloff;
    float mFormantRatio;
    float mSpectralFlatness = 0.0f;
    float mHarmonicDensity = 0.0f;
    float mVocalPresenceRatio = 0.0f;
    float mSpectralFlux = 0.0f;
    float mSpectralVariance = 0.0f;
    float mEnvelopeJitter = 0.0f;
    float mAutocorrelationIrregularity = 0.0f;
    float mZeroCrossingCV = 0.0f;
    ExponentialSmoother<float> mEnvelopeJitterSmoother{0.08f};
    ExponentialSmoother<float> mAcfIrregularitySmoother{0.08f};
    ExponentialSmoother<float> mZcCVSmoother{0.08f};
    fl::vector<float> mPrevBins;
    fl::vector<float> mFluxNormBins; // Reusable buffer for spectral flux (avoids per-frame allocation)
    SpectralVariance<float> mSpectralVarianceFilter{0.2f};
    int mSampleRate = 44100;
    int mFormantNumBins = 64;
    int mBroadNumBins = 16;
    // Cached formant bin indices for the narrow formant FFT
    int mFormantCachedBinCount = -1; // Invalidation sentinel
    int mFormantF1MinBin = 0, mFormantF1MaxBin = 0;
    int mFormantF2MinBin = 0, mFormantF2MaxBin = 0;

    shared_ptr<const fft::Bins> mRetainedFormantFFT;  // 64 bins, 200-3500 Hz
    shared_ptr<const fft::Bins> mRetainedBroadFFT;    // 16 bins, 174.6-4698.3 Hz

    // Formant ratio from high-res narrow FFT (64 bins, 200-3500 Hz)
    void computeFormantRatio(const fft::Bins& formantFft);
    // Broad spectral features from low-res wide FFT (16 bins, 174.6-4698.3 Hz)
    void computeBroadSpectralFeatures(const fft::Bins& broadFft);
    // Vocal presence ratio from broad FFT linear bins
    float calculateVocalPresenceRatio(const fft::Bins& broadFft);
    // Fused PCM pass: computes envelope jitter + shimmer AND zero-crossing CV
    // in a single traversal. Saves one full PCM pass (~2-3 us).
    void computePCMTimeDomainFeatures(span<const i16> pcm);
    float calculateAutocorrelationIrregularity(span<const i16> pcm);
    float calculateRawConfidence(float formantRatio,
                                 float spectralFlatness, float harmonicDensity,
                                 float vocalPresenceRatio, float spectralFlux,
                                 float spectralVariance);
};

// Test-only accessor for internal diagnostic state
struct VocalDetectorDiagnostics {
    static int getNumBins(const Vocal& d) { return d.mFormantNumBins; }
    static int getBroadNumBins(const Vocal& d) { return d.mBroadNumBins; }
    static float getSpectralFlatness(const Vocal& d) { return d.mSpectralFlatness; }
    static float getHarmonicDensity(const Vocal& d) { return d.mHarmonicDensity; }
    static float getSpectralCentroid(const Vocal& d) { return d.mSpectralCentroid; }
    static float getSpectralRolloff(const Vocal& d) { return d.mSpectralRolloff; }
    static float getFormantRatio(const Vocal& d) { return d.mFormantRatio; }
    static float getVocalPresenceRatio(const Vocal& d) { return d.mVocalPresenceRatio; }
    static float getSpectralFlux(const Vocal& d) { return d.mSpectralFlux; }
    static float getSpectralVariance(const Vocal& d) { return d.mSpectralVariance; }
    static float getEnvelopeJitter(const Vocal& d) { return d.mEnvelopeJitter; }
    static float getAutocorrelationIrregularity(const Vocal& d) { return d.mAutocorrelationIrregularity; }
    static float getZeroCrossingCV(const Vocal& d) { return d.mZeroCrossingCV; }
    static float getRawConfidence(const Vocal& d) { return d.mConfidence; }
};

} // namespace detector
} // namespace audio
} // namespace fl
