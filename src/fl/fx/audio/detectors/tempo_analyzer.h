#pragma once

#include "fl/audio/audio_detector.h"
#include "fl/audio/audio_context.h"
#include "fl/stl/function.h"
#include "fl/stl/vector.h"

namespace fl {

class TempoAnalyzer : public AudioDetector {
public:
    TempoAnalyzer();
    ~TempoAnalyzer() override;

    void update(shared_ptr<AudioContext> context) override;
    bool needsFFT() const override { return true; }
    bool needsFFTHistory() const override { return true; }
    const char* getName() const override { return "TempoAnalyzer"; }
    void reset() override;

    // Callbacks (multiple listeners supported)
    function_list<void(float bpm)> onTempo;
    function_list<void(float bpm, float confidence)> onTempoWithConfidence;
    function_list<void(float bpm)> onTempoChange;
    function_list<void()> onTempoStable;
    function_list<void()> onTempoUnstable;

    // State access
    float getBPM() const { return mCurrentBPM; }
    float getConfidence() const { return mConfidence; }
    bool isStable() const { return mIsStable; }
    float getStability() const { return mStability; }

    // Configuration
    void setMinBPM(float minBPM) { mMinBPM = minBPM; }
    void setMaxBPM(float maxBPM) { mMaxBPM = maxBPM; }
    void setStabilityThreshold(float threshold) { mStabilityThreshold = threshold; }

private:
    // Current tempo state
    float mCurrentBPM;
    float mConfidence;
    bool mIsStable;
    float mStability;
    float mMinBPM;
    float mMaxBPM;
    float mStabilityThreshold;

    // Tempo hypothesis tracking
    struct TempoHypothesis {
        float bpm;
        float score;
        u32 lastOnsetTime;
        u32 onsetCount;
    };
    vector<TempoHypothesis> mHypotheses;
    static constexpr size MAX_HYPOTHESES = 5;

    // Onset detection state
    vector<u32> mOnsetTimes;
    static constexpr size MAX_ONSET_HISTORY = 50;
    float mPreviousFlux;
    float mAdaptiveThreshold;
    vector<float> mFluxHistory;
    static constexpr size FLUX_HISTORY_SIZE = 43;  // ~1 second at 43fps

    // Stability tracking
    vector<float> mBPMHistory;
    static constexpr size BPM_HISTORY_SIZE = 20;
    u32 mStableFrameCount;
    static constexpr u32 STABLE_FRAMES_REQUIRED = 10;

    // Tempo range constraints
    static constexpr u32 MIN_BEAT_INTERVAL_MS = 250;  // Max 240 BPM
    static constexpr u32 MAX_BEAT_INTERVAL_MS = 2000; // Min 30 BPM

    // Internal methods
    float calculateSpectralFlux(const FFTBins& fft);
    void updateAdaptiveThreshold();
    bool detectOnset(u32 timestamp);
    void updateHypotheses(u32 timestamp);
    void pruneHypotheses();
    void updateCurrentTempo();
    void updateStability();
    float calculateIntervalScore(u32 interval);
    float calculateTempoConfidence(const TempoHypothesis& hyp);
};

} // namespace fl
