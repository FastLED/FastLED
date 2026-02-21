#include "fl/audio/detectors/beat.h"
#include "fl/audio/audio_context.h"
#include "fl/stl/math.h"
#include "fl/stl/algorithm.h"

namespace fl {

BeatDetector::BeatDetector()
    : mBeatDetected(false)
    , mBPM(120.0f)
    , mPhase(0.0f)
    , mConfidence(0.0f)
    , mThreshold(1.3f)
    , mSensitivity(1.0f)
    , mSpectralFlux(0.0f)
    , mLastBeatTime(0)
    , mBeatInterval(500)
    , mAdaptiveThreshold(0.0f)
{
    mPreviousMagnitudes.resize(16, 0.0f);
    mFluxHistory.reserve(FLUX_HISTORY_SIZE);
}

BeatDetector::~BeatDetector() = default;

void BeatDetector::update(shared_ptr<AudioContext> context) {
    const FFTBins& fft = context->getFFT(16);
    u32 timestamp = context->getTimestamp();

    // Calculate spectral flux
    mSpectralFlux = calculateSpectralFlux(fft);

    // Update adaptive threshold
    updateAdaptiveThreshold();

    // Detect beat
    mBeatDetected = detectBeat(timestamp);

    if (mBeatDetected) {
        updateTempo(timestamp);
        mLastBeatTime = timestamp;
    }

    // Update phase regardless of beat detection
    updatePhase(timestamp);

    // Update previous magnitudes for next frame
    for (size i = 0; i < fft.bins_raw.size() && i < mPreviousMagnitudes.size(); i++) {
        mPreviousMagnitudes[i] = fft.bins_raw[i];
    }
}

void BeatDetector::reset() {
    mBeatDetected = false;
    mBPM = 120.0f;
    mPhase = 0.0f;
    mConfidence = 0.0f;
    mSpectralFlux = 0.0f;
    mLastBeatTime = 0;
    mBeatInterval = 500;
    mAdaptiveThreshold = 0.0f;
    fl::fill(mPreviousMagnitudes.begin(), mPreviousMagnitudes.end(), 0.0f);
    mFluxHistory.clear();
}

float BeatDetector::calculateSpectralFlux(const FFTBins& fft) {
    float flux = 0.0f;
    size numBins = fl::fl_min(fft.bins_raw.size(), mPreviousMagnitudes.size());

    // Only consider the bass quarter of FFT bins for beat detection.
    // Musical beats (kick drums) are characterized by bass/low-mid energy.
    // With 16 bins at 44100 Hz, bins 0-3 cover 0-5512 Hz (bass + low-mid).
    // Treble transients (hi-hats, cymbals) in higher bins are excluded.
    size bassBins = numBins / 4;
    if (bassBins < 1) bassBins = 1;

    size count = 0;
    for (size i = 0; i < bassBins; i++) {
        float diff = fft.bins_raw[i] - mPreviousMagnitudes[i];
        if (diff > 0.0f) {
            flux += diff;
            count++;
        }
    }

    return (count > 0) ? flux / static_cast<float>(bassBins) : 0.0f;
}

void BeatDetector::updateAdaptiveThreshold() {
    // Add current flux to history
    if (mFluxHistory.size() >= FLUX_HISTORY_SIZE) {
        // Remove oldest
        mFluxHistory.erase(mFluxHistory.begin());
    }
    mFluxHistory.push_back(mSpectralFlux);

    // Calculate mean of flux history
    if (!mFluxHistory.empty()) {
        float sum = 0.0f;
        for (size i = 0; i < mFluxHistory.size(); i++) {
            sum += mFluxHistory[i];
        }
        float mean = sum / static_cast<float>(mFluxHistory.size());
        mAdaptiveThreshold = mean * mThreshold * mSensitivity;
    }
}

bool BeatDetector::detectBeat(u32 timestamp) {
    // Use adaptive threshold with a minimum floor to prevent triggering
    // on spectral leakage when the threshold is near zero (e.g., after silence).
    // CQ kernel magnitudes are in Q15 scale, so meaningful flux is typically 100+.
    static constexpr float MIN_FLUX_THRESHOLD = 100.0f;
    float effectiveThreshold = fl::fl_max(mAdaptiveThreshold, MIN_FLUX_THRESHOLD);

    // Check if flux exceeds effective threshold
    if (mSpectralFlux <= effectiveThreshold) {
        return false;
    }

    // Check cooldown period
    u32 timeSinceLastBeat = timestamp - mLastBeatTime;
    if (timeSinceLastBeat < MIN_BEAT_INTERVAL_MS) {
        return false;
    }

    // Calculate confidence based on how much we exceeded threshold
    if (mAdaptiveThreshold > 0.0f) {
        mConfidence = fl::fl_min(1.0f, (mSpectralFlux - mAdaptiveThreshold) / mAdaptiveThreshold);
    } else {
        mConfidence = 1.0f;
    }

    return true;
}

void BeatDetector::updateTempo(u32 timestamp) {
    u32 interval = timestamp - mLastBeatTime;

    // Only update tempo if interval is reasonable
    if (interval >= MIN_BEAT_INTERVAL_MS && interval <= MAX_BEAT_INTERVAL_MS) {
        // Smooth tempo changes
        const float alpha = 0.2f;
        mBeatInterval = static_cast<u32>(
            alpha * static_cast<float>(interval) +
            (1.0f - alpha) * static_cast<float>(mBeatInterval)
        );

        // Convert interval to BPM
        float newBPM = 60000.0f / static_cast<float>(mBeatInterval);

        // Check if tempo changed significantly
        float bpmDiff = fl::fl_abs(newBPM - mBPM);
        mTempoChanged = (bpmDiff > 5.0f);

        mBPM = newBPM;
    }
}

void BeatDetector::updatePhase(u32 timestamp) {
    if (mBeatInterval == 0) {
        mPhase = 0.0f;
        return;
    }

    u32 timeSinceLastBeat = timestamp - mLastBeatTime;
    mPhase = static_cast<float>(timeSinceLastBeat) / static_cast<float>(mBeatInterval);

    // Wrap phase to [0, 1)
    if (mPhase >= 1.0f) {
        mPhase = 1.0f - 0.001f; // Keep slightly below 1.0
    }
}

void BeatDetector::fireCallbacks() {
    if (mBeatDetected) {
        onBeat();
        onOnset(mSpectralFlux);
    }
    if (mTempoChanged) {
        onTempoChange(mBPM, mConfidence);
        mTempoChanged = false;
    }
    onBeatPhase(mPhase);
}

} // namespace fl
