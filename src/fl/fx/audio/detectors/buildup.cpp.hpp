// BuildupDetector.cpp - Implementation of EDM buildup detection

#include "fl/fx/audio/detectors/buildup.h"
#include "fl/audio/audio_context.h"
#include "fl/stl/math.h"
#include "fl/dbg.h"
#include "fl/warn.h"

namespace fl {

BuildupDetector::BuildupDetector()
    : mBuildupActive(false)
    , mPeakFired(false)
    , mEnergyHistoryIndex(0)
    , mEnergyHistorySize(0)
    , mTrebleHistoryIndex(0)
    , mTrebleHistorySize(0)
    , mPrevEnergy(0.0f)
    , mPrevTreble(0.0f)
    , mLastUpdateTime(0)
    , mMinDuration(2000)        // 2 seconds minimum
    , mMaxDuration(16000)       // 16 seconds maximum
    , mIntensityThreshold(0.6f) // 60% intensity to start
    , mEnergyRiseThreshold(0.3f) // 30% rise rate
{
    // Initialize history buffers
    for (int i = 0; i < 32; i++) {
        mEnergyHistory[i] = 0.0f;
    }
    for (int i = 0; i < 16; i++) {
        mTrebleHistory[i] = 0.0f;
    }
}

BuildupDetector::~BuildupDetector() = default;

void BuildupDetector::update(shared_ptr<AudioContext> context) {
    if (!context) {
        FL_WARN("BuildupDetector::update: null context");
        return;
    }

    const FFTBins& fft = context->getFFT(32);
    float rms = context->getRMS();
    float treble = getTrebleEnergy(fft);
    uint32_t timestamp = context->getTimestamp();

    // Update history
    updateEnergyHistory(rms);
    updateTrebleHistory(treble);

    // Calculate trends
    float energyTrend = calculateEnergyTrend();
    float trebleTrend = calculateTrebleTrend();

    // Calculate buildup intensity
    float intensity = calculateBuildupIntensity(energyTrend, trebleTrend, rms);

    if (!mBuildupActive) {
        // Check if we should start a buildup
        if (shouldStartBuildup(intensity)) {
            mBuildupActive = true;
            mPeakFired = false;
            mCurrentBuildup.timestamp = timestamp;
            mCurrentBuildup.intensity = intensity;
            mCurrentBuildup.progress = 0.0f;
            mCurrentBuildup.duration = 0;
            mCurrentBuildup.active = true;

            FL_DBG("BuildupDetector: Buildup started (intensity=" << intensity << ")");

            if (onBuildupStart) {
                onBuildupStart();
            }
        }
    } else {
        // Update existing buildup
        mCurrentBuildup.duration = timestamp - mCurrentBuildup.timestamp;
        mCurrentBuildup.intensity = intensity;

        // Calculate progress (0.0 to 1.0)
        float normalizedDuration = static_cast<float>(mCurrentBuildup.duration) / static_cast<float>(mMaxDuration);
        mCurrentBuildup.progress = fl::fl_min(1.0f, normalizedDuration);

        // Check if we've reached peak (just before drop)
        if (!mPeakFired && shouldPeak()) {
            mPeakFired = true;
            FL_DBG("BuildupDetector: Peak reached");

            if (onBuildupPeak) {
                onBuildupPeak();
            }
        }

        // Fire progress callback
        if (onBuildupProgress) {
            onBuildupProgress(mCurrentBuildup.progress);
        }

        // Fire general buildup callback
        if (onBuildup) {
            onBuildup(mCurrentBuildup);
        }

        // Check if buildup should end
        if (shouldEndBuildup()) {
            FL_DBG("BuildupDetector: Buildup ended (duration=" << mCurrentBuildup.duration << "ms)");

            mBuildupActive = false;
            mCurrentBuildup.active = false;

            if (onBuildupEnd) {
                onBuildupEnd();
            }
        }
    }

    mPrevEnergy = rms;
    mPrevTreble = treble;
    mLastUpdateTime = timestamp;
}

void BuildupDetector::reset() {
    mBuildupActive = false;
    mPeakFired = false;
    mEnergyHistoryIndex = 0;
    mEnergyHistorySize = 0;
    mTrebleHistoryIndex = 0;
    mTrebleHistorySize = 0;
    mPrevEnergy = 0.0f;
    mPrevTreble = 0.0f;
    mLastUpdateTime = 0;

    for (int i = 0; i < 32; i++) {
        mEnergyHistory[i] = 0.0f;
    }
    for (int i = 0; i < 16; i++) {
        mTrebleHistory[i] = 0.0f;
    }

    mCurrentBuildup = Buildup();
}

float BuildupDetector::calculateEnergyTrend() const {
    if (mEnergyHistorySize < 8) {
        return 0.0f;  // Not enough data
    }

    // Calculate linear regression slope (simplified)
    // Compare first half vs. second half of history
    int halfSize = mEnergyHistorySize / 2;
    float firstHalfSum = 0.0f;
    float secondHalfSum = 0.0f;

    for (int i = 0; i < halfSize; i++) {
        firstHalfSum += mEnergyHistory[i];
    }
    for (int i = halfSize; i < mEnergyHistorySize; i++) {
        secondHalfSum += mEnergyHistory[i];
    }

    float firstHalfAvg = firstHalfSum / static_cast<float>(halfSize);
    float secondHalfAvg = secondHalfSum / static_cast<float>(mEnergyHistorySize - halfSize);

    // Calculate rise rate (normalized)
    if (firstHalfAvg < 1e-6f) {
        return 0.0f;
    }

    float riseRate = (secondHalfAvg - firstHalfAvg) / firstHalfAvg;
    return fl::fl_max(0.0f, fl::fl_min(2.0f, riseRate));  // Clamp to [0, 2]
}

float BuildupDetector::calculateTrebleTrend() const {
    if (mTrebleHistorySize < 4) {
        return 0.0f;  // Not enough data
    }

    // Calculate treble rise trend
    int halfSize = mTrebleHistorySize / 2;
    float firstHalfSum = 0.0f;
    float secondHalfSum = 0.0f;

    for (int i = 0; i < halfSize; i++) {
        firstHalfSum += mTrebleHistory[i];
    }
    for (int i = halfSize; i < mTrebleHistorySize; i++) {
        secondHalfSum += mTrebleHistory[i];
    }

    float firstHalfAvg = firstHalfSum / static_cast<float>(halfSize);
    float secondHalfAvg = secondHalfSum / static_cast<float>(mTrebleHistorySize - halfSize);

    // Calculate rise rate
    if (firstHalfAvg < 1e-6f) {
        return 0.0f;
    }

    float riseRate = (secondHalfAvg - firstHalfAvg) / firstHalfAvg;
    return fl::fl_max(0.0f, fl::fl_min(2.0f, riseRate));  // Clamp to [0, 2]
}

float BuildupDetector::calculateBuildupIntensity(float energyTrend, float trebleTrend, float rms) const {
    // Buildup intensity is a combination of:
    // - Energy rise trend (50%)
    // - Treble rise trend (30%)
    // - Overall energy level (20%)

    float normalizedEnergy = fl::fl_min(1.0f, rms);
    float normalizedEnergyTrend = energyTrend / 2.0f;  // Normalize from [0, 2] to [0, 1]
    float normalizedTrebleTrend = trebleTrend / 2.0f;

    float intensity = normalizedEnergyTrend * 0.5f +
                      normalizedTrebleTrend * 0.3f +
                      normalizedEnergy * 0.2f;

    return fl::fl_max(0.0f, fl::fl_min(1.0f, intensity));
}

bool BuildupDetector::shouldStartBuildup(float intensity) const {
    // Start buildup if:
    // 1. Intensity exceeds threshold
    // 2. Energy is rising (positive trend)
    // 3. We have enough history

    if (mEnergyHistorySize < 8) {
        return false;  // Not enough data
    }

    float energyTrend = calculateEnergyTrend();

    return intensity >= mIntensityThreshold &&
           energyTrend >= mEnergyRiseThreshold;
}

bool BuildupDetector::shouldEndBuildup() const {
    if (!mBuildupActive) {
        return false;
    }

    // End buildup if:
    // 1. Duration exceeded maximum
    // 2. Energy drops significantly (sudden energy loss = cancelled buildup)
    // 3. Intensity drops below threshold for sustained period

    if (mCurrentBuildup.duration > mMaxDuration) {
        return true;  // Too long
    }

    // Check for sudden energy drop (drop cancelled)
    float energyTrend = calculateEnergyTrend();
    if (energyTrend < -0.5f) {  // Significant energy drop
        return true;
    }

    // Check if intensity dropped below threshold
    if (mCurrentBuildup.intensity < mIntensityThreshold * 0.5f) {
        return true;  // Lost momentum
    }

    return false;
}

bool BuildupDetector::shouldPeak() const {
    if (!mBuildupActive || mPeakFired) {
        return false;
    }

    // Peak when:
    // 1. Duration is at least minimum
    // 2. Progress is near 1.0 (85%+)
    // 3. Intensity is very high (90%+)
    // OR
    // 4. We've reached maximum duration

    bool durationOk = mCurrentBuildup.duration >= mMinDuration;
    bool nearEnd = mCurrentBuildup.progress >= 0.85f;
    bool highIntensity = mCurrentBuildup.intensity >= 0.9f;
    bool atMax = mCurrentBuildup.duration >= mMaxDuration * 0.95f;

    return durationOk && (nearEnd || highIntensity || atMax);
}

void BuildupDetector::updateEnergyHistory(float energy) {
    mEnergyHistory[mEnergyHistoryIndex] = energy;
    mEnergyHistoryIndex = (mEnergyHistoryIndex + 1) % 32;

    if (mEnergyHistorySize < 32) {
        mEnergyHistorySize++;
    }
}

void BuildupDetector::updateTrebleHistory(float treble) {
    mTrebleHistory[mTrebleHistoryIndex] = treble;
    mTrebleHistoryIndex = (mTrebleHistoryIndex + 1) % 16;

    if (mTrebleHistorySize < 16) {
        mTrebleHistorySize++;
    }
}

float BuildupDetector::getTrebleEnergy(const FFTBins& fft) const {
    // Calculate high-frequency energy (top 25% of bins)
    int startBin = static_cast<int>(fft.bins_raw.size() * 0.75f);
    float energy = 0.0f;
    int count = 0;

    for (fl::size i = startBin; i < fft.bins_raw.size(); i++) {
        energy += fft.bins_raw[i];
        count++;
    }

    return (count > 0) ? energy / static_cast<float>(count) : 0.0f;
}

} // namespace fl
