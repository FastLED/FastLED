#pragma once

#include "fl/audio/audio_context.h"
#include "fl/stl/function.h"

namespace fl {

class VocalDetector {
public:
    VocalDetector();
    ~VocalDetector();

    void update(shared_ptr<AudioContext> context);
    void reset();

    // Vocal state change callbacks (multiple listeners supported)
    function_list<void(bool active)> onVocalChange;  // Called when vocal state changes
    function_list<void()> onVocalStart;       // Called when vocals start
    function_list<void()> onVocalEnd;         // Called when vocals end

    // Configuration
    void setThreshold(float threshold) { mThreshold = threshold; }

    // Getters
    bool isVocalActive() const { return mVocalActive; }
    float getConfidence() const { return mConfidence; }
    float getSpectralCentroid() const { return mSpectralCentroid; }
    float getSpectralRolloff() const { return mSpectralRolloff; }
    float getFormantRatio() const { return mFormantRatio; }

private:
    // Vocal detection state
    bool mVocalActive;
    bool mPreviousVocalActive;
    float mConfidence;
    float mThreshold;

    // Spectral features
    float mSpectralCentroid;    // Measure of frequency distribution
    float mSpectralRolloff;     // Frequency point where energy is concentrated
    float mFormantRatio;        // Relationship between key formant frequencies

    // Spectral feature calculations
    float calculateSpectralCentroid(const FFTBins& fft);
    float calculateSpectralRolloff(const FFTBins& fft);
    float estimateFormantRatio(const FFTBins& fft);

    // Vocal detection algorithm
    bool detectVocal(float centroid, float rolloff, float formantRatio);
};

} // namespace fl
