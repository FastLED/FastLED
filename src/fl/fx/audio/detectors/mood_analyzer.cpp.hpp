// MoodAnalyzer - Mood and emotion detection from audio features
// Part of FastLED Audio Processing System (Phase 3 - Tier 3)

#include "fl/fx/audio/detectors/mood_analyzer.h"
#include "fl/audio/audio_context.h"
#include "fl/stl/math.h"

namespace fl {

MoodAnalyzer::MoodAnalyzer()
    : mConfidenceThreshold(0.5f)
    , mMinDuration(1500)  // 1.5 seconds minimum
    , mAveragingFrames(10)
    , mSpectralCentroid(0.0f)
    , mSpectralRolloff(0.0f)
    , mSpectralFlux(0.0f)
    , mZeroCrossingRate(0.0f)
    , mRMSEnergy(0.0f)
    , mHistoryIndex(0)
{
    mValenceHistory.reserve(mAveragingFrames);
    mArousalHistory.reserve(mAveragingFrames);
}

MoodAnalyzer::~MoodAnalyzer() = default;

void MoodAnalyzer::update(shared_ptr<AudioContext> context) {
    const FFTBins& fft = context->getFFT(32);  // Higher resolution for mood analysis
    const FFTBins* prevFFT = context->getHistoricalFFT(1);

    // Extract audio features
    mSpectralCentroid = calculateSpectralCentroid(fft);
    mSpectralRolloff = calculateSpectralRolloff(fft);
    mSpectralFlux = calculateSpectralFlux(fft, prevFFT);
    mZeroCrossingRate = context->getZCF();
    mRMSEnergy = context->getRMS();

    // Calculate mood dimensions
    float valence = calculateValence(mSpectralCentroid, mSpectralRolloff, mSpectralFlux);
    float arousal = calculateArousal(mRMSEnergy, mZeroCrossingRate, mSpectralFlux);

    // Add to history for temporal averaging
    if (static_cast<int>(mValenceHistory.size()) < mAveragingFrames) {
        mValenceHistory.push_back(valence);
        mArousalHistory.push_back(arousal);
        mHistoryIndex = mValenceHistory.size();
    } else {
        mValenceHistory[mHistoryIndex] = valence;
        mArousalHistory[mHistoryIndex] = arousal;
        mHistoryIndex = (mHistoryIndex + 1) % mAveragingFrames;
    }

    // Average over history for stability
    float avgValence = 0.0f;
    float avgArousal = 0.0f;
    for (size_t i = 0; i < mValenceHistory.size(); i++) {
        avgValence += mValenceHistory[i];
        avgArousal += mArousalHistory[i];
    }
    avgValence /= mValenceHistory.size();
    avgArousal /= mArousalHistory.size();

    // Update current mood
    mPreviousMood = mCurrentMood;
    mCurrentMood.valence = avgValence;
    mCurrentMood.arousal = avgArousal;
    mCurrentMood.confidence = calculateConfidence(avgValence, avgArousal);
    mCurrentMood.timestamp = context->getTimestamp();

    // Update duration if mood is stable
    if (mPreviousMood.getCategory() == mCurrentMood.getCategory()) {
        mCurrentMood.duration = mPreviousMood.duration +
            (mCurrentMood.timestamp - mPreviousMood.timestamp);
    } else {
        mCurrentMood.duration = 0;
    }

    // Fire callbacks
    if (onMood) {
        onMood(mCurrentMood);
    }

    if (onValenceArousal) {
        onValenceArousal(mCurrentMood.valence, mCurrentMood.arousal);
    }

    // Check for mood changes
    if (shouldChangeMood(mCurrentMood) && onMoodChange) {
        onMoodChange(mCurrentMood);
    }
}

void MoodAnalyzer::reset() {
    mCurrentMood = Mood();
    mPreviousMood = Mood();
    mSpectralCentroid = 0.0f;
    mSpectralRolloff = 0.0f;
    mSpectralFlux = 0.0f;
    mZeroCrossingRate = 0.0f;
    mRMSEnergy = 0.0f;
    mValenceHistory.clear();
    mArousalHistory.clear();
    mHistoryIndex = 0;
}

float MoodAnalyzer::calculateSpectralCentroid(const FFTBins& fft) {
    float weightedSum = 0.0f;
    float magnitudeSum = 0.0f;

    for (size_t i = 0; i < fft.bins_raw.size(); i++) {
        float magnitude = fft.bins_raw[i];
        weightedSum += i * magnitude;
        magnitudeSum += magnitude;
    }

    return (magnitudeSum < 1e-6f) ? 0.0f : weightedSum / magnitudeSum;
}

float MoodAnalyzer::calculateSpectralRolloff(const FFTBins& fft, float threshold) {
    float totalEnergy = 0.0f;

    for (size_t i = 0; i < fft.bins_raw.size(); i++) {
        float magnitude = fft.bins_raw[i];
        totalEnergy += magnitude * magnitude;
    }

    float energyThreshold = totalEnergy * threshold;
    float cumulativeEnergy = 0.0f;

    for (size_t i = 0; i < fft.bins_raw.size(); i++) {
        float magnitude = fft.bins_raw[i];
        cumulativeEnergy += magnitude * magnitude;
        if (cumulativeEnergy >= energyThreshold) {
            return static_cast<float>(i) / fft.bins_raw.size();
        }
    }

    return 1.0f;
}

float MoodAnalyzer::calculateSpectralFlux(const FFTBins& fft, const FFTBins* prevFFT) {
    if (!prevFFT || prevFFT->bins_raw.size() != fft.bins_raw.size()) {
        return 0.0f;
    }

    float flux = 0.0f;
    for (size_t i = 0; i < fft.bins_raw.size(); i++) {
        float diff = fft.bins_raw[i] - prevFFT->bins_raw[i];
        flux += diff * diff;
    }

    return fl::sqrt(flux);
}

float MoodAnalyzer::calculateValence(float centroid, float rolloff, float flux) {
    // Valence estimation based on spectral characteristics
    // Higher frequencies and brighter timbre = more positive
    // Lower frequencies and darker timbre = more negative

    // Normalize centroid to 0-1 range (assuming 32 bins)
    float normalizedCentroid = centroid / 32.0f;

    // Brightness score (higher = more positive)
    float brightness = normalizedCentroid * rolloff;

    // Stability score (lower flux = more positive/calm, higher = more negative/chaotic)
    float stability = 1.0f - fl::fl_min(1.0f, flux / 10.0f);

    // Combine into valence (-1 to 1)
    float valence = (brightness * 0.6f + stability * 0.4f) * 2.0f - 1.0f;

    // Clamp to valid range
    return fl::fl_max(-1.0f, fl::fl_min(1.0f, valence));
}

float MoodAnalyzer::calculateArousal(float rms, float zcr, float flux) {
    // Arousal estimation based on energy and dynamics
    // Higher energy and more change = higher arousal
    // Lower energy and less change = lower arousal

    // Normalize RMS (assuming typical range 0-1)
    float normalizedRMS = fl::fl_min(1.0f, rms);

    // Normalize ZCR (assuming typical range 0-1)
    float normalizedZCR = fl::fl_min(1.0f, zcr);

    // Normalize flux (assuming typical range 0-10)
    float normalizedFlux = fl::fl_min(1.0f, flux / 10.0f);

    // Combine into arousal (0 to 1)
    float arousal = normalizedRMS * 0.5f + normalizedZCR * 0.2f + normalizedFlux * 0.3f;

    // Clamp to valid range
    return fl::fl_max(0.0f, fl::fl_min(1.0f, arousal));
}

float MoodAnalyzer::calculateConfidence(float valence, float arousal) {
    // Confidence based on distance from neutral (center)
    // Further from neutral = higher confidence

    float distanceFromNeutral = fl::sqrt(valence * valence + arousal * arousal);

    // For valence: range is -1 to 1, so max distance from 0 is 1
    // For arousal: range is 0 to 1, so max distance from 0.5 is 0.5
    // Combined max distance is approximately sqrt(1^2 + 0.5^2) = 1.118
    float normalizedDistance = distanceFromNeutral / 1.118f;

    // Clamp to valid range
    return fl::fl_max(0.0f, fl::fl_min(1.0f, normalizedDistance));
}

bool MoodAnalyzer::shouldChangeMood(const Mood& newMood) {
    // Check if mood category has changed
    if (mPreviousMood.getCategory() == newMood.getCategory()) {
        return false;
    }

    // Check confidence threshold
    if (newMood.confidence < mConfidenceThreshold) {
        return false;
    }

    // Check minimum duration (unless first mood or previous was invalid)
    if (mPreviousMood.isValid() && mPreviousMood.duration < mMinDuration) {
        // Allow early change if new mood is significantly more confident
        float confidenceRatio = newMood.confidence / (mPreviousMood.confidence + 0.01f);
        if (confidenceRatio < 1.3f) {
            return false;
        }
    }

    return true;
}

} // namespace fl
