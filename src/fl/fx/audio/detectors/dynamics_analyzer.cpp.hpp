#include "fl/fx/audio/detectors/dynamics_analyzer.h"
#include "fl/audio/audio_context.h"
#include "fl/stl/math.h"

namespace fl {

DynamicsAnalyzer::DynamicsAnalyzer()
    : mHistorySize(86)  // ~2 seconds at 43fps
    , mHistoryIndex(0)
    , mCurrentRMS(0.0f)
    , mAverageRMS(0.0f)
    , mPeakRMS(0.0f)
    , mMinRMS(1.0f)
    , mTrend(0.0f)
    , mCompressionRatio(1.0f)
    , mPeakDecay(0.99f)
    , mSmoothingFactor(0.9f)
    , mTrendThreshold(0.15f)
    , mIsCrescendo(false)
    , mIsDiminuendo(false)
    , mPrevIsCrescendo(false)
    , mPrevIsDiminuendo(false)
    , mLastUpdateTime(0)
{
    mRMSHistory.reserve(mHistorySize);
}

DynamicsAnalyzer::~DynamicsAnalyzer() = default;

void DynamicsAnalyzer::update(shared_ptr<AudioContext> context) {
    mCurrentRMS = context->getRMS();
    u32 timestamp = context->getTimestamp();

    // Update peak with decay
    updatePeak(mCurrentRMS);

    // Add to history
    if (mRMSHistory.size() < mHistorySize) {
        mRMSHistory.push_back(mCurrentRMS);
        mHistoryIndex = mRMSHistory.size();
    } else {
        mRMSHistory[mHistoryIndex] = mCurrentRMS;
        mHistoryIndex = (mHistoryIndex + 1) % mHistorySize;
    }

    // Calculate average RMS from history
    float sum = 0.0f;
    for (fl::size i = 0; i < mRMSHistory.size(); i++) {
        sum += mRMSHistory[i];
    }
    mAverageRMS = (mRMSHistory.size() > 0) ? sum / mRMSHistory.size() : 0.0f;

    // Track minimum RMS (with slow rise to handle changing environments)
    if (mCurrentRMS < mMinRMS) {
        mMinRMS = mCurrentRMS;
    } else {
        mMinRMS += (mCurrentRMS - mMinRMS) * 0.001f;  // Very slow rise
    }

    // Calculate dynamic trend
    float newTrend = calculateTrend();

    // Smooth the trend
    mTrend = mSmoothingFactor * mTrend + (1.0f - mSmoothingFactor) * newTrend;

    // Determine crescendo/diminuendo state
    mPrevIsCrescendo = mIsCrescendo;
    mPrevIsDiminuendo = mIsDiminuendo;

    mIsCrescendo = (mTrend > mTrendThreshold);
    mIsDiminuendo = (mTrend < -mTrendThreshold);

    // Fire callbacks for state changes
    if (mIsCrescendo && !mPrevIsCrescendo && onCrescendo) {
        onCrescendo();
    }

    if (mIsDiminuendo && !mPrevIsDiminuendo && onDiminuendo) {
        onDiminuendo();
    }

    // Fire trend callback (always)
    if (onDynamicTrend) {
        onDynamicTrend(mTrend);
    }

    // Update compression ratio
    updateCompression();

    // Fire compression callback
    if (onCompressionRatio) {
        onCompressionRatio(mCompressionRatio);
    }

    mLastUpdateTime = timestamp;
}

void DynamicsAnalyzer::reset() {
    mRMSHistory.clear();
    mHistoryIndex = 0;
    mCurrentRMS = 0.0f;
    mAverageRMS = 0.0f;
    mPeakRMS = 0.0f;
    mMinRMS = 1.0f;
    mTrend = 0.0f;
    mCompressionRatio = 1.0f;
    mIsCrescendo = false;
    mIsDiminuendo = false;
    mPrevIsCrescendo = false;
    mPrevIsDiminuendo = false;
    mLastUpdateTime = 0;
}

float DynamicsAnalyzer::calculateTrend() {
    if (mRMSHistory.size() < 10) {
        return 0.0f;  // Not enough data
    }

    // Use linear regression to determine trend
    // Compare recent average to older average
    fl::size halfSize = mRMSHistory.size() / 2;

    float recentSum = 0.0f;
    float olderSum = 0.0f;

    // Recent half
    for (fl::size i = 0; i < halfSize; i++) {
        fl::size idx = (mHistoryIndex + i) % mRMSHistory.size();
        olderSum += mRMSHistory[idx];
    }

    // Older half
    for (fl::size i = halfSize; i < mRMSHistory.size(); i++) {
        fl::size idx = (mHistoryIndex + i) % mRMSHistory.size();
        recentSum += mRMSHistory[idx];
    }

    float recentAvg = recentSum / halfSize;
    float olderAvg = olderSum / halfSize;

    // Avoid division by zero
    if (olderAvg < 1e-6f) {
        return 0.0f;
    }

    // Calculate normalized trend (-1 to +1)
    float rawTrend = (recentAvg - olderAvg) / olderAvg;

    // Clamp to reasonable range
    return fl::fl_max(-1.0f, fl::fl_min(1.0f, rawTrend * 5.0f));
}

void DynamicsAnalyzer::updatePeak(float rms) {
    if (rms > mPeakRMS) {
        mPeakRMS = rms;
    } else {
        // Apply decay
        mPeakRMS *= mPeakDecay;
    }
}

void DynamicsAnalyzer::updateCompression() {
    // Calculate dynamic range compression ratio
    // Compression ratio = (peak - min) / peak
    // High compression (near 1.0) = small dynamic range
    // Low compression (near 0.0) = large dynamic range

    if (mPeakRMS < 1e-6f) {
        mCompressionRatio = 1.0f;
        return;
    }

    float dynamicRange = mPeakRMS - mMinRMS;
    mCompressionRatio = 1.0f - (dynamicRange / mPeakRMS);

    // Clamp to 0-1
    mCompressionRatio = fl::fl_max(0.0f, fl::fl_min(1.0f, mCompressionRatio));
}

void DynamicsAnalyzer::setHistorySize(fl::size size) {
    mHistorySize = size;
    mRMSHistory.clear();
    mRMSHistory.reserve(size);
    mHistoryIndex = 0;
}

void DynamicsAnalyzer::setTrendThreshold(float threshold) {
    mTrendThreshold = threshold;
}

void DynamicsAnalyzer::setSmoothingFactor(float alpha) {
    // Clamp to 0-1
    mSmoothingFactor = fl::fl_max(0.0f, fl::fl_min(1.0f, alpha));
}

}  // namespace fl
