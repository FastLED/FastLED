#pragma once

#include "fl/audio/audio_detector.h"
#include "fl/audio/audio_context.h"
#include "fl/function.h"
#include "fl/vector.h"

namespace fl {

// DynamicsAnalyzer tracks loudness trends over time to detect crescendos,
// diminuendos, and overall dynamic evolution.
class DynamicsAnalyzer : public AudioDetector {
public:
    DynamicsAnalyzer();
    ~DynamicsAnalyzer() override;

    // AudioDetector interface
    void update(shared_ptr<AudioContext> context) override;
    bool needsFFT() const override { return false; }
    const char* getName() const override { return "DynamicsAnalyzer"; }
    void reset() override;

    // Callbacks
    function<void()> onCrescendo;          // Loudness increasing
    function<void()> onDiminuendo;         // Loudness decreasing
    function<void(float trend)> onDynamicTrend;  // Current trend (-1 to +1)
    function<void(float compression)> onCompressionRatio;  // Dynamic range compression

    // Configuration
    void setHistorySize(fl::size size);
    void setTrendThreshold(float threshold);
    void setSmoothingFactor(float alpha);

    // State access
    float getDynamicTrend() const { return mTrend; }
    float getCurrentRMS() const { return mCurrentRMS; }
    float getAverageRMS() const { return mAverageRMS; }
    float getPeakRMS() const { return mPeakRMS; }
    float getCompressionRatio() const { return mCompressionRatio; }
    bool isCrescendo() const { return mIsCrescendo; }
    bool isDiminuendo() const { return mIsDiminuendo; }

private:
    vector<float> mRMSHistory;
    fl::size mHistorySize;
    fl::size mHistoryIndex;

    float mCurrentRMS;
    float mAverageRMS;
    float mPeakRMS;
    float mMinRMS;
    float mTrend;
    float mCompressionRatio;
    float mPeakDecay;
    float mSmoothingFactor;
    float mTrendThreshold;

    bool mIsCrescendo;
    bool mIsDiminuendo;
    bool mPrevIsCrescendo;
    bool mPrevIsDiminuendo;

    u32 mLastUpdateTime;

    float calculateTrend();
    void updatePeak(float rms);
    void updateCompression();
};

}  // namespace fl
