#pragma once

#include "fl/audio/audio_detector.h"
#include "fl/math/filter/filter.h"
#include "fl/stl/function.h"
#include "fl/stl/vector.h"
#include "fl/stl/noexcept.h"

namespace fl {
namespace audio {
namespace detector {

/**
 * Pitch - Continuous pitch tracking using autocorrelation
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
class Pitch : public Detector {
public:
    Pitch() FL_NO_EXCEPT;
    ~Pitch() FL_NO_EXCEPT override;

    void update(shared_ptr<Context> context) FL_NO_EXCEPT override;
    void fireCallbacks() FL_NO_EXCEPT override;
    bool needsFFT() const FL_NO_EXCEPT override { return false; }  // Uses PCM data directly
    const char* getName() const FL_NO_EXCEPT override { return "Pitch"; }
    void reset() FL_NO_EXCEPT override;

    // Callbacks (multiple listeners supported)
    function_list<void(float hz)> onPitch;  // Continuous pitch updates
    function_list<void(float hz, float confidence)> onPitchWithConfidence;
    function_list<void(float hz)> onPitchChange;  // Fires when pitch changes significantly
    function_list<void(u8 voiced)> onVoiced;  // Fires when voiced/unvoiced state changes

    // State access
    float getPitch() const FL_NO_EXCEPT { return mCurrentPitch; }
    float getConfidence() const FL_NO_EXCEPT { return mConfidence; }
    bool isVoiced() const FL_NO_EXCEPT { return mIsVoiced; }  // True if pitched sound detected
    float getSmoothedPitch() const FL_NO_EXCEPT { return mSmoothedPitch; }

    // Configuration
    void setMinFrequency(float hz) FL_NO_EXCEPT { mMinFrequency = hz; updatePeriodRange(); }
    void setMaxFrequency(float hz) FL_NO_EXCEPT { mMaxFrequency = hz; updatePeriodRange(); }
    void setConfidenceThreshold(float threshold) FL_NO_EXCEPT { mConfidenceThreshold = threshold; }
    void setSmoothingFactor(float) FL_NO_EXCEPT { /* OneEuroFilter adapts automatically */ }
    void setPitchChangeSensitivity(float sensitivity) FL_NO_EXCEPT { mPitchChangeSensitivity = sensitivity; }

private:
    // Current state
    float mCurrentPitch;      // Current detected pitch in Hz
    float mSmoothedPitch;     // Exponentially smoothed pitch
    float mConfidence;        // Detection confidence (0-1)
    bool mIsVoiced;           // True if currently detecting pitched sound
    bool mPreviousVoiced;     // Previous voiced state for change detection
    float mPreviousPitch;     // Previous pitch for change detection
    bool mFirePitch = false;
    bool mFirePitchChange = false;
    bool mVoicedStateChanged = false;

    // Configuration
    float mMinFrequency;      // Minimum detectable frequency (Hz)
    float mMaxFrequency;      // Maximum detectable frequency (Hz)
    float mConfidenceThreshold;  // Minimum confidence to report pitch
    float mPitchChangeSensitivity;  // Sensitivity for pitch change detection (Hz)

    // Adaptive pitch smoothing: low jitter when stable, low lag on changes
    OneEuroFilter<float> mPitchSmoother{1.0f, 0.5f};
    float mLastDt = 0.023f; // Computed from pcmSize / sampleRate each frame

    // Autocorrelation parameters
    int mMinPeriod;           // Minimum period in samples
    int mMaxPeriod;           // Maximum period in samples
    float mSampleRate;        // Audio sample rate (Hz)

    // Autocorrelation buffer
    vector<float> mAutocorrelation;

    // Helper methods
    void updatePeriodRange() FL_NO_EXCEPT;
    float calculateAutocorrelation(const i16* pcm, size numSamples) FL_NO_EXCEPT;
    float periodToFrequency(int period) const FL_NO_EXCEPT;
    int frequencyToPeriod(float frequency) const FL_NO_EXCEPT;
    float calculateConfidence(const vector<float>& autocorr, int peakLag) const FL_NO_EXCEPT;
    int findBestPeakLag(const vector<float>& autocorr) const FL_NO_EXCEPT;
    void updatePitchSmoothing(float newPitch) FL_NO_EXCEPT;
    bool shouldReportPitchChange(float newPitch) const FL_NO_EXCEPT;
};

} // namespace detector
} // namespace audio
} // namespace fl
