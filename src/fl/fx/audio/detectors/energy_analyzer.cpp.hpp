#include "fl/fx/audio/detectors/energy_analyzer.h"
#include "fl/audio/audio_context.h"
#include "fl/stl/math.h"

namespace fl {

EnergyAnalyzer::EnergyAnalyzer()
    : mCurrentRMS(0.0f)
    , mPeak(0.0f)
    , mAverageEnergy(0.0f)
    , mMinEnergy(1e6f)
    , mMaxEnergy(0.0f)
    , mPeakDecay(0.95f)
    , mLastPeakTime(0)
    , mHistorySize(43)  // ~1 second at 43fps
    , mHistoryIndex(0)
{
    mEnergyHistory.reserve(mHistorySize);
}

EnergyAnalyzer::~EnergyAnalyzer() = default;

void EnergyAnalyzer::update(shared_ptr<AudioContext> context) {
    // Get RMS directly from AudioSample (no FFT needed)
    mCurrentRMS = context->getRMS();
    u32 timestamp = context->getTimestamp();

    // Update peak tracking
    updatePeak(mCurrentRMS, timestamp);

    // Update average and history
    updateAverage(mCurrentRMS);

    // Update min/max
    if (mCurrentRMS > 0.001f) {  // Ignore near-silence
        mMinEnergy = fl::fl_min(mMinEnergy, mCurrentRMS);
        mMaxEnergy = fl::fl_max(mMaxEnergy, mCurrentRMS);
    }

    // Fire callbacks
    if (onEnergy) {
        onEnergy(mCurrentRMS);
    }
    if (onPeak) {
        onPeak(mPeak);
    }
    if (onAverageEnergy) {
        onAverageEnergy(mAverageEnergy);
    }
}

void EnergyAnalyzer::reset() {
    mCurrentRMS = 0.0f;
    mPeak = 0.0f;
    mAverageEnergy = 0.0f;
    mMinEnergy = 1e6f;
    mMaxEnergy = 0.0f;
    mLastPeakTime = 0;
    mEnergyHistory.clear();
    mHistoryIndex = 0;
}

void EnergyAnalyzer::setHistorySize(int size) {
    if (size != mHistorySize) {
        mHistorySize = size;
        mEnergyHistory.clear();
        mEnergyHistory.reserve(size);
        mHistoryIndex = 0;
    }
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
    if (static_cast<int>(mEnergyHistory.size()) < mHistorySize) {
        // Still building up history
        mEnergyHistory.push_back(energy);
    } else {
        // Ring buffer mode
        mEnergyHistory[mHistoryIndex] = energy;
        mHistoryIndex = (mHistoryIndex + 1) % mHistorySize;
    }

    // Calculate average
    float sum = 0.0f;
    for (size i = 0; i < mEnergyHistory.size(); i++) {
        sum += mEnergyHistory[i];
    }
    mAverageEnergy = sum / static_cast<float>(mEnergyHistory.size());
}

} // namespace fl
