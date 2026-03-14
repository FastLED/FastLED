// VocalDetector - Human voice detection using spectral characteristics
// Part of FastLED Audio System v2.0 - Phase 3 (Differentiators)
//
// Detects human voice in audio using spectral centroid, spectral rolloff,
// and formant ratio analysis. Provides confidence-based detection with
// hysteresis for stable vocal/non-vocal classification.

#pragma once

#include "fl/audio/audio_detector.h"
#include "fl/filter.h"
#include "fl/stl/math.h"
#include "fl/stl/function.h"
#include "fl/stl/shared_ptr.h"

namespace fl {

class VocalDetector : public AudioDetector {
public:
    VocalDetector();
    ~VocalDetector() override;

    // AudioDetector interface
    void update(shared_ptr<AudioContext> context) override;
    void fireCallbacks() override;
    bool needsFFT() const override { return true; }
    const char* getName() const override { return "VocalDetector"; }
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
    SpectralVariance<float> mSpectralVarianceFilter{0.2f};
    int mSampleRate = 44100;
    int mNumBins = 128;

    shared_ptr<const FFTBins> mRetainedFFT;

    // Analysis methods
    float calculateSpectralCentroid(const FFTBins& fft);
    float calculateSpectralRolloff(const FFTBins& fft);
    float estimateFormantRatio(const FFTBins& fft);
    float calculateSpectralFlatness(const FFTBins& fft);
    float calculateHarmonicDensity(const FFTBins& fft);
    float calculateVocalPresenceRatio(const FFTBins& fft);
    float calculateSpectralFlux(const FFTBins& fft);
    float calculateSpectralVariance(const FFTBins& fft);
    float calculateEnvelopeJitter(span<const i16> pcm);
    float calculateAutocorrelationIrregularity(span<const i16> pcm);
    float calculateZeroCrossingCV(span<const i16> pcm);
    float calculateRawConfidence(float centroid, float rolloff, float formantRatio,
                                 float spectralFlatness, float harmonicDensity,
                                 float vocalPresenceRatio, float spectralFlux,
                                 float spectralVariance);
};

// Test-only accessor for internal diagnostic state
struct VocalDetectorDiagnostics {
    static int getNumBins(const VocalDetector& d) { return d.mNumBins; }
    static float getSpectralFlatness(const VocalDetector& d) { return d.mSpectralFlatness; }
    static float getHarmonicDensity(const VocalDetector& d) { return d.mHarmonicDensity; }
    static float getSpectralCentroid(const VocalDetector& d) { return d.mSpectralCentroid; }
    static float getSpectralRolloff(const VocalDetector& d) { return d.mSpectralRolloff; }
    static float getFormantRatio(const VocalDetector& d) { return d.mFormantRatio; }
    static float getVocalPresenceRatio(const VocalDetector& d) { return d.mVocalPresenceRatio; }
    static float getSpectralFlux(const VocalDetector& d) { return d.mSpectralFlux; }
    static float getSpectralVariance(const VocalDetector& d) { return d.mSpectralVariance; }
    static float getEnvelopeJitter(const VocalDetector& d) { return d.mEnvelopeJitter; }
    static float getAutocorrelationIrregularity(const VocalDetector& d) { return d.mAutocorrelationIrregularity; }
    static float getZeroCrossingCV(const VocalDetector& d) { return d.mZeroCrossingCV; }
    static float getRawConfidence(const VocalDetector& d) { return d.mConfidence; }
};

} // namespace fl
