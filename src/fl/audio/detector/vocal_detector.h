#pragma once

#include "fl/stl/function.h"
#include "fl/stl/noexcept.h"

namespace fl {
namespace audio {
namespace detector {

class Vocal {
public:
    Vocal() FL_NOEXCEPT;
    ~Vocal() FL_NOEXCEPT;

    void update(shared_ptr<Context> context);
    void reset();

    // Vocal state change callbacks (multiple listeners supported)
    function_list<void(u8 active)> onVocal;  // Called when vocal state changes
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
    float calculateSpectralCentroid(const fft::Bins& fft);
    float calculateSpectralRolloff(const fft::Bins& fft);
    float estimateFormantRatio(const fft::Bins& fft);

    // Vocal detection algorithm
    bool detectVocal(float centroid, float rolloff, float formantRatio);
};

} // namespace detector
} // namespace audio
} // namespace fl
