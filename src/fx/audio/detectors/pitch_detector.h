#pragma once

#include "fl/stl/vector.h"
#include "fl/audio/audio_context.h"

namespace fl {

class PitchDetector {
public:
    // Constructors and destructor
    PitchDetector();
    ~PitchDetector();

    // Update method
    void update(shared_ptr<AudioContext> context);
    void reset();

    // Callback types
    using PitchCallback = void(*)(float pitch);
    using PitchConfidenceCallback = void(*)(float pitch, float confidence);
    using VoicedCallback = void(*)(bool isVoiced);

    // Pitch-related callbacks
    PitchCallback onPitch = nullptr;                          // Called continuously when pitch is detected
    PitchConfidenceCallback onPitchWithConfidence = nullptr;   // Pitch with confidence score
    PitchCallback onPitchChange = nullptr;                    // Called when pitch changes significantly
    VoicedCallback onVoicedChange = nullptr;                  // Called when voiced/unvoiced state changes

    // Configuration methods
    void setMinFrequency(float hz) { mMinFrequency = hz; updatePeriodRange(); }
    void setMaxFrequency(float hz) { mMaxFrequency = hz; updatePeriodRange(); }
    void setConfidenceThreshold(float threshold) { mConfidenceThreshold = threshold; }
    void setSmoothingFactor(float factor) { mSmoothingFactor = factor; }
    void setPitchChangeSensitivity(float sensitivity) { mPitchChangeSensitivity = sensitivity; }
    void setSampleRate(float rate) { mSampleRate = rate; updatePeriodRange(); }

    // Getters
    float getPitch() const { return mSmoothedPitch; }
    float getConfidence() const { return mConfidence; }
    bool isVoiced() const { return mIsVoiced; }

private:
    // Pitch tracking variables
    float mCurrentPitch;
    float mSmoothedPitch;
    float mConfidence;
    bool mIsVoiced;
    bool mPreviousVoiced;
    float mPreviousPitch;

    // Detection configuration
    float mMinFrequency;      // Minimum detectable frequency
    float mMaxFrequency;      // Maximum detectable frequency
    float mConfidenceThreshold;  // Minimum confidence to consider pitch valid
    float mSmoothingFactor;   // Pitch smoothing factor
    float mPitchChangeSensitivity;  // Threshold for pitch change events

    // Sample rate and period information
    float mSampleRate;
    int mMinPeriod;
    int mMaxPeriod;

    // Autocorrelation buffer
    fl::vector<float> mAutocorrelation;

    // Internal helper methods
    void updatePeriodRange();
    float calculateAutocorrelation(const int16_t* pcm, size numSamples);
    int findBestPeakLag(const fl::vector<float>& autocorr) const;
    float calculateConfidence(const fl::vector<float>& autocorr, int peakLag) const;
    float periodToFrequency(int period) const;
    int frequencyToPeriod(float frequency) const;
    void updatePitchSmoothing(float newPitch);
    bool shouldReportPitchChange(float newPitch) const;
};

} // namespace fl
