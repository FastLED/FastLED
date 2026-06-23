#include "fl/audio/detector/energy_analyzer.h"
#include "fl/audio/audio_context.h"
#include "fl/math/math.h"
#include "fl/stl/noexcept.h"

namespace fl {
namespace audio {
namespace detector {

EnergyAnalyzer::EnergyAnalyzer()
    : mCurrentRMS(0.0f)
    , mPeak(0.0f)
    , mAverageEnergy(0.0f)
    , mMinEnergy(1e6f)
    , mMaxEnergy(0.0f)
    , mPeakDecay(0.95f)
    , mLastPeakTime(0)
{
}

EnergyAnalyzer::~EnergyAnalyzer() FL_NOEXCEPT = default;

void EnergyAnalyzer::update(shared_ptr<Context> context) {
    // Get RMS directly from Sample (no fft::FFT needed)
    mCurrentRMS = context->getRMS();
    u32 timestamp = context->getTimestamp();

    // Update peak tracking
    updatePeak(mCurrentRMS, timestamp);

    // Update average and history
    updateAverage(mCurrentRMS);

    // Update min/max
    if (mCurrentRMS > 0.001f) {  // Ignore near-silence
        mMinEnergy = fl::min(mMinEnergy, mCurrentRMS);
        mMaxEnergy = fl::max(mMaxEnergy, mCurrentRMS);
    }

    // Compute normalized 0-1 RMS using adaptive range tracking.
    // AttackDecayFilter: instant attack (0.001s), slow decay (2.0s).
    const float dt = computeAudioDt(context->getPCM().size(), context->getSampleRate());
    float runningMax = mRunningMaxFilter.update(mCurrentRMS, dt);
    // Ensure running max doesn't decay below a minimum threshold
    if (runningMax < 1.0f) {
        runningMax = 1.0f;
    }
    mNormalizedRMS = fl::min(1.0f, mCurrentRMS / runningMax);
}

void EnergyAnalyzer::fireCallbacks() {
    if (onEnergy) {
        onEnergy(mCurrentRMS);
    }
    if (onPeak) {
        onPeak(mPeak);
    }
    if (onAverageEnergy) {
        onAverageEnergy(mAverageEnergy);
    }
    if (onNormalizedEnergy) {
        onNormalizedEnergy(mNormalizedRMS);
    }
}

void EnergyAnalyzer::reset() {
    mCurrentRMS = 0.0f;
    mPeak = 0.0f;
    mAverageEnergy = 0.0f;
    mMinEnergy = 1e6f;
    mMaxEnergy = 0.0f;
    mNormalizedRMS = 0.0f;
    mRunningMaxFilter.reset(1.0f);
    mLastPeakTime = 0;
    mEnergyAvg.reset();
}

void EnergyAnalyzer::setHistorySize(int size) {
    mEnergyAvg.resize(static_cast<fl::size>(size));
}

void EnergyAnalyzer::updatePeak(float energy, u32 timestamp) {
    // Check if we should decay the peak
    u32 timeSincePeak = timestamp - mLastPeakTime;

    if (energy > mPeak) {
        // New peak
        mPeak = energy;
        mLastPeakTime = timestamp;
    } else if (timeSincePeak > PEAK_HOLD_MS) {
        // Decay the peak
        mPeak *= mPeakDecay;

        // Ensure peak doesn't go below current energy
        if (mPeak < energy) {
            mPeak = energy;
            mLastPeakTime = timestamp;
        }
    }
}

void EnergyAnalyzer::updateAverage(float energy) {
    mEnergyAvg.update(energy);
    mAverageEnergy = mEnergyAvg.value();
}

} // namespace detector
} // namespace audio
} // namespace fl
