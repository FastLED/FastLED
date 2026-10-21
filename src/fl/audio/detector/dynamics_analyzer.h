#pragma once

#include "fl/audio/audio_detector.h"
#include "fl/stl/function.h"
#include "fl/stl/vector.h"
#include "fl/stl/noexcept.h"

namespace fl {
namespace audio {
namespace detector {

// DynamicsAnalyzer tracks loudness trends over time to detect crescendos,
// diminuendos, and overall dynamic evolution.
class DynamicsAnalyzer : public Detector {
public:
    DynamicsAnalyzer() FL_NO_EXCEPT;
    ~DynamicsAnalyzer() FL_NO_EXCEPT override;

    // Detector interface
    void update(shared_ptr<Context> context) FL_NO_EXCEPT override;
    void fireCallbacks() FL_NO_EXCEPT override;
    bool needsFFT() const FL_NO_EXCEPT override { return false; }
    const char* getName() const FL_NO_EXCEPT override { return "DynamicsAnalyzer"; }
    void reset() FL_NO_EXCEPT override;

    // Callbacks (multiple listeners supported)
    function_list<void()> onCrescendo;          // Loudness increasing
    function_list<void()> onDiminuendo;         // Loudness decreasing
    function_list<void(float trend)> onDynamicTrend;  // Current trend (-1 to +1)
    function_list<void(float compression)> onCompressionRatio;  // Dynamic range compression

    // Configuration
    void setHistorySize(fl::size size) FL_NO_EXCEPT;
    void setTrendThreshold(float threshold) FL_NO_EXCEPT;
    void setSmoothingFactor(float alpha) FL_NO_EXCEPT;

    // State access
    float getDynamicTrend() const FL_NO_EXCEPT { return mTrend; }
    float getCurrentRMS() const FL_NO_EXCEPT { return mCurrentRMS; }
    float getAverageRMS() const FL_NO_EXCEPT { return mAverageRMS; }
    float getPeakRMS() const FL_NO_EXCEPT { return mPeakRMS; }
    float getCompressionRatio() const FL_NO_EXCEPT { return mCompressionRatio; }
    bool isCrescendo() const FL_NO_EXCEPT { return mIsCrescendo; }
    bool isDiminuendo() const FL_NO_EXCEPT { return mIsDiminuendo; }

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

    float calculateTrend() FL_NO_EXCEPT;
    void updatePeak(float rms) FL_NO_EXCEPT;
    void updateCompression() FL_NO_EXCEPT;
};

} // namespace detector
} // namespace audio
}  // namespace fl
