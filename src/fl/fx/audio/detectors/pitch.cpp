#include "fl/fx/audio/detectors/pitch.h"
#include "fl/audio/audio_context.h"
#include "fl/stl/math.h"

namespace fl {

PitchDetector::PitchDetector()
    : mCurrentPitch(0.0f)
    , mSmoothedPitch(0.0f)
    , mConfidence(0.0f)
    , mIsVoiced(false)
    , mPreviousVoiced(false)
    , mPreviousPitch(0.0f)
    , mMinFrequency(80.0f)     // Typical low male voice / bass guitar
    , mMaxFrequency(1000.0f)   // Upper range for most melodic instruments
    , mConfidenceThreshold(0.5f)  // Require 50% confidence minimum
    , mSmoothingFactor(0.85f)  // High smoothing for stable pitch
    , mPitchChangeSensitivity(5.0f)  // 5 Hz threshold for pitch change events
    , mMinPeriod(0)
    , mMaxPeriod(0)
    , mSampleRate(44100.0f)    // Standard audio sample rate
{
    updatePeriodRange();
    // Reserve space for autocorrelation buffer (worst case: max period)
    mAutocorrelation.reserve(static_cast<size>(mMaxPeriod + 1));
}

PitchDetector::~PitchDetector() = default;

void PitchDetector::update(shared_ptr<AudioContext> context) {
    // Get PCM data from context
    span<const int16_t> pcm = context->getPCM();
    size numSamples = pcm.size();

    // Need at least 2x max period for autocorrelation
    if (numSamples < static_cast<size>(mMaxPeriod * 2)) {
        // Not enough samples for reliable pitch detection
        mConfidence = 0.0f;
        mIsVoiced = false;

        if (mIsVoiced != mPreviousVoiced) {
            if (onVoicedChange) {
                onVoicedChange(mIsVoiced);
            }
            mPreviousVoiced = mIsVoiced;
        }
        return;
    }

    // Calculate autocorrelation and find pitch
    float detectedPitch = calculateAutocorrelation(pcm.data(), numSamples);

    // Check if pitch is valid and confidence is sufficient
    if (detectedPitch > 0.0f && mConfidence >= mConfidenceThreshold) {
        mIsVoiced = true;
        mCurrentPitch = detectedPitch;

        // Apply exponential smoothing
        updatePitchSmoothing(detectedPitch);

        // Fire continuous pitch callbacks
        if (onPitch) {
            onPitch(mSmoothedPitch);
        }
        if (onPitchWithConfidence) {
            onPitchWithConfidence(mSmoothedPitch, mConfidence);
        }

        // Check for pitch change
        if (shouldReportPitchChange(detectedPitch)) {
            if (onPitchChange) {
                onPitchChange(mSmoothedPitch);
            }
            mPreviousPitch = detectedPitch;
        }
    } else {
        // No reliable pitch detected
        mIsVoiced = false;
        mCurrentPitch = 0.0f;
    }

    // Fire voiced state change callback
    if (mIsVoiced != mPreviousVoiced) {
        if (onVoicedChange) {
            onVoicedChange(mIsVoiced);
        }
        mPreviousVoiced = mIsVoiced;
    }
}

void PitchDetector::reset() {
    mCurrentPitch = 0.0f;
    mSmoothedPitch = 0.0f;
    mConfidence = 0.0f;
    mIsVoiced = false;
    mPreviousVoiced = false;
    mPreviousPitch = 0.0f;
    mAutocorrelation.clear();
}

void PitchDetector::updatePeriodRange() {
    // Convert frequency range to period range (in samples)
    // Period (samples) = SampleRate / Frequency
    mMinPeriod = frequencyToPeriod(mMaxFrequency);
    mMaxPeriod = frequencyToPeriod(mMinFrequency);
}

float PitchDetector::calculateAutocorrelation(const int16_t* pcm, size numSamples) {
    // Clear and resize autocorrelation buffer
    mAutocorrelation.clear();
    mAutocorrelation.resize(static_cast<size>(mMaxPeriod + 1), 0.0f);

    // Normalize input to float range [-1, 1]
    const float normFactor = 1.0f / 32768.0f;

    // Calculate autocorrelation for all lags in the period range
    // ACF[k] = sum(signal[n] * signal[n + k]) for all valid n
    for (int lag = mMinPeriod; lag <= mMaxPeriod; lag++) {
        float sum = 0.0f;
        int validSamples = 0;

        for (size i = 0; i + lag < numSamples; i++) {
            float s1 = static_cast<float>(pcm[i]) * normFactor;
            float s2 = static_cast<float>(pcm[i + lag]) * normFactor;
            sum += s1 * s2;
            validSamples++;
        }

        // Normalize by number of samples
        if (validSamples > 0) {
            mAutocorrelation[static_cast<size>(lag)] = sum / static_cast<float>(validSamples);
        }
    }

    // Find the lag with maximum autocorrelation (best period match)
    int bestLag = findBestPeakLag(mAutocorrelation);

    // Calculate confidence based on autocorrelation peak
    if (bestLag > 0) {
        mConfidence = calculateConfidence(mAutocorrelation, bestLag);

        // Convert period (lag) to frequency
        return periodToFrequency(bestLag);
    }

    // No reliable pitch found
    mConfidence = 0.0f;
    return 0.0f;
}

int PitchDetector::findBestPeakLag(const vector<float>& autocorr) const {
    // Find the lag with maximum autocorrelation value
    // (excluding lag 0, which is always maximum by definition)

    float maxValue = -1.0f;
    int bestLag = 0;

    for (int lag = mMinPeriod; lag <= mMaxPeriod && lag < static_cast<int>(autocorr.size()); lag++) {
        float value = autocorr[static_cast<size>(lag)];

        // Look for positive peaks
        if (value > maxValue && value > 0.0f) {
            maxValue = value;
            bestLag = lag;
        }
    }

    return bestLag;
}

float PitchDetector::calculateConfidence(const vector<float>& autocorr, int peakLag) const {
    // Confidence based on:
    // 1. Strength of the autocorrelation peak
    // 2. Ratio of peak to nearby values (peak clarity)

    if (peakLag <= 0 || peakLag >= static_cast<int>(autocorr.size())) {
        return 0.0f;
    }

    float peakValue = autocorr[static_cast<size>(peakLag)];

    // Confidence is primarily based on peak strength
    // Autocorrelation ranges from -1 to 1, but we only care about positive peaks
    float confidence = fl::fl_max(0.0f, fl::fl_min(1.0f, peakValue));

    // Calculate clarity by comparing peak to surrounding values
    // Look at ±10% of the period
    int windowSize = fl::fl_max(2, peakLag / 10);
    float neighborSum = 0.0f;
    int neighborCount = 0;

    for (int offset = -windowSize; offset <= windowSize; offset++) {
        if (offset == 0) continue;  // Skip the peak itself

        int lag = peakLag + offset;
        if (lag >= mMinPeriod && lag <= mMaxPeriod && lag < static_cast<int>(autocorr.size())) {
            neighborSum += fl::fl_max(0.0f, autocorr[static_cast<size>(lag)]);
            neighborCount++;
        }
    }

    if (neighborCount > 0) {
        float neighborAvg = neighborSum / static_cast<float>(neighborCount);

        // Peak should be significantly higher than neighbors
        // If peak is 2x higher, that's good; if it's similar, reduce confidence
        if (neighborAvg > 1e-6f) {
            float clarity = fl::fl_min(1.0f, (peakValue - neighborAvg) / neighborAvg);
            confidence *= (0.7f + 0.3f * clarity);  // Weight clarity at 30%
        }
    }

    return confidence;
}

float PitchDetector::periodToFrequency(int period) const {
    if (period <= 0) {
        return 0.0f;
    }
    return mSampleRate / static_cast<float>(period);
}

int PitchDetector::frequencyToPeriod(float frequency) const {
    if (frequency <= 0.0f) {
        return 0;
    }
    return static_cast<int>(mSampleRate / frequency);
}

void PitchDetector::updatePitchSmoothing(float newPitch) {
    if (mSmoothedPitch == 0.0f) {
        // First pitch detection - initialize smoothed pitch
        mSmoothedPitch = newPitch;
    } else {
        // Exponential moving average
        // smoothed = alpha * smoothed + (1 - alpha) * new
        mSmoothedPitch = mSmoothingFactor * mSmoothedPitch +
                        (1.0f - mSmoothingFactor) * newPitch;
    }
}

bool PitchDetector::shouldReportPitchChange(float newPitch) const {
    // Check if pitch has changed significantly from previous detection
    if (mPreviousPitch == 0.0f) {
        // First pitch detection
        return true;
    }

    // Calculate absolute difference in Hz
    float pitchDiff = fl::fl_abs(newPitch - mPreviousPitch);

    // Report change if difference exceeds sensitivity threshold
    return pitchDiff >= mPitchChangeSensitivity;
}

} // namespace fl
