// BuildupDetector.cpp - Implementation of EDM buildup detection

#include "fl/audio/detector/buildup.h"
#include "fl/audio/audio_context.h"
#include "fl/math/math.h"
#include "fl/system/log.h"
#include "fl/system/log.h"
#include "fl/stl/noexcept.h"

namespace fl {
namespace audio {
namespace detector {

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

BuildupDetector::~BuildupDetector() FL_NOEXCEPT = default;

void BuildupDetector::update(shared_ptr<Context> context) {
    if (!context) {
        FL_WARN("BuildupDetector::update: null context");
        return;
    }

    mRetainedFFT = context->getFFT(32);
    const fft::Bins& fft = *mRetainedFFT;
    float rms = context->getRMS();
    float treble = getTrebleEnergy(fft);
    u32 timestamp = context->getTimestamp();

    // Update history and SG filters
    updateEnergyHistory(rms);
    updateTrebleHistory(treble);
    mEnergySG.update(rms);
    mTrebleSG.update(treble);

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

            mFireBuildupStart = true;
        }
    } else {
        // Update existing buildup
        mCurrentBuildup.duration = timestamp - mCurrentBuildup.timestamp;
        mCurrentBuildup.intensity = intensity;

        // Calculate progress (0.0 to 1.0)
        float normalizedDuration = static_cast<float>(mCurrentBuildup.duration) / static_cast<float>(mMaxDuration);
        mCurrentBuildup.progress = fl::min(1.0f, normalizedDuration);

        // Check if we've reached peak (just before drop)
        if (!mPeakFired && shouldPeak()) {
            mPeakFired = true;
            FL_DBG("BuildupDetector: Peak reached");

            mFireBuildupPeak = true;
        }

        // Set progress callback flag
        mFireBuildupProgress = true;

        // Set general buildup callback flag
        mFireBuildup = true;

        // Check if buildup should end
        if (shouldEndBuildup()) {
            FL_DBG("BuildupDetector: Buildup ended (duration=" << mCurrentBuildup.duration << "ms)");

            mBuildupActive = false;
            mCurrentBuildup.active = false;

            mFireBuildupEnd = true;
        }
    }

    mPrevEnergy = rms;
    mPrevTreble = treble;
    mLastUpdateTime = timestamp;
}

void BuildupDetector::fireCallbacks() {
    if (mFireBuildupStart) {
        if (onBuildupStart) onBuildupStart();
        mFireBuildupStart = false;
    }
    if (mFireBuildupPeak) {
        if (onBuildupPeak) onBuildupPeak();
        mFireBuildupPeak = false;
    }
    if (mFireBuildupProgress) {
        if (onBuildupProgress) onBuildupProgress(mCurrentBuildup.progress);
        mFireBuildupProgress = false;
    }
    if (mFireBuildup) {
        if (onBuildup) onBuildup(mCurrentBuildup);
        mFireBuildup = false;
    }
    if (mFireBuildupEnd) {
        if (onBuildupEnd) onBuildupEnd();
        mFireBuildupEnd = false;
    }
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
    mEnergySG.reset();
    mTrebleSG.reset();

    mCurrentBuildup = Buildup();
}

float BuildupDetector::calculateEnergyTrend() const {
    if (mEnergyHistorySize < 8 || !mEnergySG.full()) {
        return 0.0f;  // Not enough data
    }

    // Use SG-smoothed current value vs oldest history entry for trend
    float currentSmoothed = mEnergySG.value();
    // Get the oldest entry in the circular buffer
    int oldestIdx = (mEnergyHistorySize < 32) ? 0 : mEnergyHistoryIndex;
    float oldestValue = mEnergyHistory[oldestIdx];

    if (oldestValue < 1e-6f) {
        return 0.0f;
    }

    float riseRate = (currentSmoothed - oldestValue) / oldestValue;
    return fl::max(0.0f, fl::min(2.0f, riseRate));  // Clamp to [0, 2]
}

float BuildupDetector::calculateTrebleTrend() const {
    if (mTrebleHistorySize < 4 || !mTrebleSG.full()) {
        return 0.0f;  // Not enough data
    }

    // Use SG-smoothed current value vs oldest history entry for trend
    float currentSmoothed = mTrebleSG.value();
    int oldestIdx = (mTrebleHistorySize < 16) ? 0 : mTrebleHistoryIndex;
    float oldestValue = mTrebleHistory[oldestIdx];

    if (oldestValue < 1e-6f) {
        return 0.0f;
    }

    float riseRate = (currentSmoothed - oldestValue) / oldestValue;
    return fl::max(0.0f, fl::min(2.0f, riseRate));  // Clamp to [0, 2]
}

float BuildupDetector::calculateBuildupIntensity(float energyTrend, float trebleTrend, float rms) const {
    // Buildup intensity is a combination of:
    // - Energy rise trend (50%)
    // - Treble rise trend (30%)
    // - Overall energy level (20%)

    float normalizedEnergy = fl::min(1.0f, rms);
    float normalizedEnergyTrend = energyTrend / 2.0f;  // Normalize from [0, 2] to [0, 1]
    float normalizedTrebleTrend = trebleTrend / 2.0f;

    float intensity = normalizedEnergyTrend * 0.5f +
                      normalizedTrebleTrend * 0.3f +
                      normalizedEnergy * 0.2f;

    return fl::max(0.0f, fl::min(1.0f, intensity));
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

float BuildupDetector::getTrebleEnergy(const fft::Bins& fft) const {
    // Calculate high-frequency energy (top 25% of bins)
    int startBin = static_cast<int>(fft.raw().size() * 0.75f);
    float energy = 0.0f;
    int count = 0;

    for (fl::size i = startBin; i < fft.raw().size(); i++) {
        energy += fft.raw()[i];
        count++;
    }

    return (count > 0) ? energy / static_cast<float>(count) : 0.0f;
}

} // namespace detector
} // namespace audio
} // namespace fl
