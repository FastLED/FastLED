#pragma once

#include "fl/audio/audio_detector.h"
#include "fl/audio/audio_context.h"
#include "fl/stl/function.h"
#include "fl/stl/vector.h"

namespace fl {

/**
 * PitchDetector - Continuous pitch tracking using autocorrelation
 *
 * Detects the fundamental frequency (pitch) of audio signals using autocorrelation
 * analysis on the time-domain PCM data. This detector provides continuous pitch
 * tracking with configurable confidence thresholds, smoothing, and stability.
 *
 * Key Features:
 * - Autocorrelation-based pitch detection (time-domain analysis)
 * - Configurable pitch range (default: 80-1000 Hz)
 * - Confidence-based filtering to reject unreliable detections
 * - Exponential smoothing for stable pitch output
 * - Pitch change detection with configurable sensitivity
 * - Support for both voiced (pitched) and unvoiced (unpitched) audio
 *
 * Performance:
 * - No FFT required (uses raw PCM data)
 * - Update time: ~0.2-0.5ms per frame
 * - Memory: ~100 bytes + autocorrelation buffer (~2KB for 512 samples)
 */
class PitchDetector : public AudioDetector {
public:
    PitchDetector();
    ~PitchDetector() override;

    void update(shared_ptr<AudioContext> context) override;
    bool needsFFT() const override { return false; }  // Uses PCM data directly
    const char* getName() const override { return "PitchDetector"; }
    void reset() override;

    // Callbacks (multiple listeners supported)
    function_list<void(float hz)> onPitch;  // Continuous pitch updates
    function_list<void(float hz, float confidence)> onPitchWithConfidence;
    function_list<void(float hz)> onPitchChange;  // Fires when pitch changes significantly
    function_list<void(bool voiced)> onVoicedChange;  // Fires when voiced/unvoiced state changes

    // State access
    float getPitch() const { return mCurrentPitch; }
    float getConfidence() const { return mConfidence; }
    bool isVoiced() const { return mIsVoiced; }  // True if pitched sound detected
    float getSmoothedPitch() const { return mSmoothedPitch; }

    // Configuration
    void setMinFrequency(float hz) { mMinFrequency = hz; updatePeriodRange(); }
    void setMaxFrequency(float hz) { mMaxFrequency = hz; updatePeriodRange(); }
    void setConfidenceThreshold(float threshold) { mConfidenceThreshold = threshold; }
    void setSmoothingFactor(float alpha) { mSmoothingFactor = alpha; }
    void setPitchChangeSensitivity(float sensitivity) { mPitchChangeSensitivity = sensitivity; }

private:
    // Current state
    float mCurrentPitch;      // Current detected pitch in Hz
    float mSmoothedPitch;     // Exponentially smoothed pitch
    float mConfidence;        // Detection confidence (0-1)
    bool mIsVoiced;           // True if currently detecting pitched sound
    bool mPreviousVoiced;     // Previous voiced state for change detection
    float mPreviousPitch;     // Previous pitch for change detection

    // Configuration
    float mMinFrequency;      // Minimum detectable frequency (Hz)
    float mMaxFrequency;      // Maximum detectable frequency (Hz)
    float mConfidenceThreshold;  // Minimum confidence to report pitch
    float mSmoothingFactor;   // Exponential smoothing alpha (0-1)
    float mPitchChangeSensitivity;  // Sensitivity for pitch change detection (Hz)

    // Autocorrelation parameters
    int mMinPeriod;           // Minimum period in samples
    int mMaxPeriod;           // Maximum period in samples
    float mSampleRate;        // Audio sample rate (Hz)

    // Autocorrelation buffer
    vector<float> mAutocorrelation;

    // Helper methods
    void updatePeriodRange();
    float calculateAutocorrelation(const int16_t* pcm, size numSamples);
    float periodToFrequency(int period) const;
    int frequencyToPeriod(float frequency) const;
    float calculateConfidence(const vector<float>& autocorr, int peakLag) const;
    int findBestPeakLag(const vector<float>& autocorr) const;
    void updatePitchSmoothing(float newPitch);
    bool shouldReportPitchChange(float newPitch) const;
};

} // namespace fl
