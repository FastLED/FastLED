// VocalDetector - Human voice detection using spectral characteristics
// Part of FastLED Audio System v2.0 - Phase 3 (Differentiators)
//
// Detects human voice in audio using spectral centroid, spectral rolloff,
// and formant ratio analysis. Provides confidence-based detection with
// hysteresis for stable vocal/non-vocal classification.

#pragma once

#include "fl/audio/audio_detector.h"
#include "fl/audio/audio_context.h"
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
    float getConfidence() const { return mConfidence; }
    void setThreshold(float threshold) { mThreshold = threshold; }
    int getNumBins() const { return mNumBins; }

private:
    bool mVocalActive;
    bool mPreviousVocalActive;
    bool mStateChanged = false;
    float mConfidence;
    float mThreshold;
    float mSpectralCentroid;
    float mSpectralRolloff;
    float mFormantRatio;
    int mSampleRate = 44100;
    int mNumBins = 128;

    // Analysis methods
    float calculateSpectralCentroid(const FFTBins& fft);
    float calculateSpectralRolloff(const FFTBins& fft);
    float estimateFormantRatio(const FFTBins& fft);
    bool detectVocal(float centroid, float rolloff, float formantRatio);
};

} // namespace fl
