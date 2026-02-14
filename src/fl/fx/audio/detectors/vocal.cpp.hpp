// VocalDetector - Human voice detection implementation
// Part of FastLED Audio System v2.0 - Phase 3 (Differentiators)

#include "fl/fx/audio/detectors/vocal.h"
#include "fl/audio/audio_context.h"
#include "fl/stl/math.h"

namespace fl {

VocalDetector::VocalDetector()
    : mVocalActive(false)
    , mPreviousVocalActive(false)
    , mConfidence(0.0f)
    , mThreshold(0.65f)
    , mSpectralCentroid(0.0f)
    , mSpectralRolloff(0.0f)
    , mFormantRatio(0.0f)
{}

VocalDetector::~VocalDetector() = default;

void VocalDetector::update(shared_ptr<AudioContext> context) {
    mSampleRate = context->getSampleRate();
    const FFTBins& fft = context->getFFT(128);
    mNumBins = static_cast<int>(fft.bins_raw.size());

    // Calculate spectral features
    mSpectralCentroid = calculateSpectralCentroid(fft);
    mSpectralRolloff = calculateSpectralRolloff(fft);
    mFormantRatio = estimateFormantRatio(fft);

    // Detect vocal based on spectral characteristics
    mVocalActive = detectVocal(mSpectralCentroid, mSpectralRolloff, mFormantRatio);

    // Fire callbacks on state changes
    if (mVocalActive != mPreviousVocalActive) {
        if (onVocalChange) onVocalChange(mVocalActive);
        if (mVocalActive && onVocalStart) onVocalStart();
        if (!mVocalActive && onVocalEnd) onVocalEnd();
        mPreviousVocalActive = mVocalActive;
    }
}

void VocalDetector::reset() {
    mVocalActive = false;
    mPreviousVocalActive = false;
    mConfidence = 0.0f;
    mSpectralCentroid = 0.0f;
    mSpectralRolloff = 0.0f;
    mFormantRatio = 0.0f;
}

float VocalDetector::calculateSpectralCentroid(const FFTBins& fft) {
    float weightedSum = 0.0f;
    float magnitudeSum = 0.0f;

    for (fl::size i = 0; i < fft.bins_raw.size(); i++) {
        float magnitude = fft.bins_raw[i];
        weightedSum += i * magnitude;
        magnitudeSum += magnitude;
    }

    return (magnitudeSum < 1e-6f) ? 0.0f : weightedSum / magnitudeSum;
}

float VocalDetector::calculateSpectralRolloff(const FFTBins& fft) {
    const float rolloffThreshold = 0.85f;
    float totalEnergy = 0.0f;

    // Calculate total energy
    for (fl::size i = 0; i < fft.bins_raw.size(); i++) {
        float magnitude = fft.bins_raw[i];
        totalEnergy += magnitude * magnitude;
    }

    float energyThreshold = totalEnergy * rolloffThreshold;
    float cumulativeEnergy = 0.0f;

    // Find rolloff point
    for (fl::size i = 0; i < fft.bins_raw.size(); i++) {
        float magnitude = fft.bins_raw[i];
        cumulativeEnergy += magnitude * magnitude;
        if (cumulativeEnergy >= energyThreshold) {
            return static_cast<float>(i) / fft.bins_raw.size();
        }
    }

    return 1.0f;
}

float VocalDetector::estimateFormantRatio(const FFTBins& fft) {
    if (fft.bins_raw.size() < 8) return 0.0f;

    // Calculate bin-to-frequency mapping from actual sample rate
    const float nyquist = static_cast<float>(mSampleRate) / 2.0f;
    const int numBins = static_cast<int>(fft.bins_raw.size());
    const float hzPerBin = nyquist / static_cast<float>(numBins);

    // F1 range: 500-900 Hz (first vocal formant)
    const int f1MinBin = fl::fl_max(0, static_cast<int>(500.0f / hzPerBin));
    const int f1MaxBin = fl::fl_min(numBins - 1, static_cast<int>(900.0f / hzPerBin));

    // F2 range: 1200-2400 Hz (second vocal formant)
    const int f2MinBin = fl::fl_max(0, static_cast<int>(1200.0f / hzPerBin));
    const int f2MaxBin = fl::fl_min(numBins - 1, static_cast<int>(2400.0f / hzPerBin));

    // Find peak energy in F1 range
    float f1Energy = 0.0f;
    for (int i = f1MinBin; i <= f1MaxBin && i < numBins; i++) {
        f1Energy = fl::fl_max(f1Energy, fft.bins_raw[i]);
    }

    // Find peak energy in F2 range
    float f2Energy = 0.0f;
    for (int i = f2MinBin; i <= f2MaxBin && i < numBins; i++) {
        f2Energy = fl::fl_max(f2Energy, fft.bins_raw[i]);
    }

    return (f1Energy < 1e-6f) ? 0.0f : f2Energy / f1Energy;
}

bool VocalDetector::detectVocal(float centroid, float rolloff, float formantRatio) {
    // Normalize centroid to 0-1 range using actual bin count
    float normalizedCentroid = centroid / static_cast<float>(mNumBins);

    // Check if features fall within vocal ranges
    // Human voice typically has:
    // - Spectral centroid: 0.3-0.7 (mid-frequency focused)
    // - Spectral rolloff: 0.5-0.8 (energy concentrated in lower-mid frequencies)
    // - Formant ratio: 0.8-2.0 (characteristic F1/F2 relationship)
    bool centroidOk = (normalizedCentroid >= 0.3f && normalizedCentroid <= 0.7f);
    bool rolloffOk = (rolloff >= 0.5f && rolloff <= 0.8f);
    bool formantOk = (formantRatio >= 0.8f && formantRatio <= 2.0f);

    // Calculate confidence scores for each feature
    float centroidScore = 1.0f - fl::fl_abs(normalizedCentroid - 0.5f) * 2.0f;
    float rolloffScore = 1.0f - fl::fl_abs(rolloff - 0.65f) / 0.35f;
    float formantScore = (formantOk) ? 1.0f : 0.0f;

    // Overall confidence is average of all scores
    mConfidence = (centroidScore + rolloffScore + formantScore) / 3.0f;

    // Detect vocal if all features are in range and confidence is above threshold
    return (centroidOk && rolloffOk && formantOk && mConfidence >= mThreshold);
}

} // namespace fl
