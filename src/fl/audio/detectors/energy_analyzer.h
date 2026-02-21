#pragma once

#include "fl/audio/audio_detector.h"
#include "fl/audio/audio_context.h"
#include "fl/stl/function.h"
#include "fl/stl/vector.h"

namespace fl {

class EnergyAnalyzer : public AudioDetector {
public:
    EnergyAnalyzer();
    ~EnergyAnalyzer() override;

    void update(shared_ptr<AudioContext> context) override;
    void fireCallbacks() override;
    bool needsFFT() const override { return false; }  // Uses RMS from AudioSample
    const char* getName() const override { return "EnergyAnalyzer"; }
    void reset() override;

    // Callbacks (multiple listeners supported)
    function_list<void(float rms)> onEnergy;
    function_list<void(float peak)> onPeak;
    function_list<void(float avgEnergy)> onAverageEnergy;
    function_list<void(float normalizedRms)> onNormalizedEnergy;  // 0-1 range

    // State access
    float getRMS() const { return mCurrentRMS; }
    float getPeak() const { return mPeak; }
    float getAverageEnergy() const { return mAverageEnergy; }
    float getMinEnergy() const { return mMinEnergy; }
    float getMaxEnergy() const { return mMaxEnergy; }
    float getNormalizedRMS() const { return mNormalizedRMS; }  // 0-1 range

    // Configuration
    void setPeakDecay(float decay) { mPeakDecay = decay; }
    void setHistorySize(int size);

private:
    float mCurrentRMS;
    float mPeak;
    float mAverageEnergy;
    float mMinEnergy;
    float mMaxEnergy;
    float mNormalizedRMS = 0.0f;

    // Adaptive range tracking for normalization
    float mRunningMax = 1.0f;        // Adaptive ceiling (decays slowly)
    static constexpr float MAX_DECAY = 0.999f;  // Slow decay for ceiling

    // Peak tracking
    float mPeakDecay;
    u32 mLastPeakTime;
    static constexpr u32 PEAK_HOLD_MS = 50;

    // History for average calculation
    vector<float> mEnergyHistory;
    int mHistorySize;
    int mHistoryIndex;

    void updatePeak(float energy, u32 timestamp);
    void updateAverage(float energy);
};

} // namespace fl
