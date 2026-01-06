#pragma once

#include "fl/audio/audio_detector.h"
#include "fl/audio/audio_context.h"
#include "fl/stl/function.h"
#include "fl/stl/vector.h"

namespace fl {

class BeatDetector : public AudioDetector {
public:
    BeatDetector();
    ~BeatDetector() override;

    void update(shared_ptr<AudioContext> context) override;
    bool needsFFT() const override { return true; }
    bool needsFFTHistory() const override { return true; }
    const char* getName() const override { return "BeatDetector"; }
    void reset() override;

    // Callbacks (multiple listeners supported)
    function_list<void()> onBeat;
    function_list<void(float phase)> onBeatPhase;
    function_list<void(float strength)> onOnset;
    function_list<void(float bpm, float confidence)> onTempoChange;

    // State access
    bool isBeat() const { return mBeatDetected; }
    float getBPM() const { return mBPM; }
    float getPhase() const { return mPhase; }
    float getConfidence() const { return mConfidence; }

    // Configuration
    void setThreshold(float threshold) { mThreshold = threshold; }
    void setSensitivity(float sensitivity) { mSensitivity = sensitivity; }

private:
    bool mBeatDetected;
    float mBPM;
    float mPhase;
    float mConfidence;
    float mThreshold;
    float mSensitivity;

    // Spectral flux tracking
    vector<float> mPreviousMagnitudes;
    float mSpectralFlux;

    // Temporal tracking
    u32 mLastBeatTime;
    u32 mBeatInterval;
    static constexpr u32 MIN_BEAT_INTERVAL_MS = 250;  // Max 240 BPM
    static constexpr u32 MAX_BEAT_INTERVAL_MS = 2000; // Min 30 BPM

    // Adaptive threshold
    float mAdaptiveThreshold;
    vector<float> mFluxHistory;
    static constexpr size FLUX_HISTORY_SIZE = 43;  // ~1 second at 43fps

    float calculateSpectralFlux(const FFTBins& fft);
    void updateAdaptiveThreshold();
    bool detectBeat(u32 timestamp);
    void updateTempo(u32 timestamp);
    void updatePhase(u32 timestamp);
};

} // namespace fl
