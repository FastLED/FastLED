#pragma once

#include "fl/audio/audio_detector.h"
#include "fl/math/filter/filter.h"
#include "fl/stl/function.h"
#include "fl/stl/noexcept.h"

namespace fl {
namespace audio {
namespace detector {

class EnergyAnalyzer : public Detector {
public:
    EnergyAnalyzer() FL_NOEXCEPT;
    ~EnergyAnalyzer() FL_NOEXCEPT override;

    void update(shared_ptr<Context> context) override;
    void fireCallbacks() override;
    bool needsFFT() const override { return false; }  // Uses RMS from Sample
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

    // Adaptive range tracking for normalization (instant attack, slow 2s decay)
    AttackDecayFilter<float> mRunningMaxFilter{0.001f, 2.0f, 1.0f};

    // Peak tracking
    float mPeakDecay;
    u32 mLastPeakTime;
    static constexpr u32 PEAK_HOLD_MS = 50;

    // History for average calculation (O(1) running sum)
    MovingAverage<float, 0> mEnergyAvg{43};

    void updatePeak(float energy, u32 timestamp);
    void updateAverage(float energy);
};

} // namespace detector
} // namespace audio
} // namespace fl
