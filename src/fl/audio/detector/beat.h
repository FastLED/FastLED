#pragma once

#include "fl/audio/audio_detector.h"
#include "fl/math/filter/filter.h"
#include "fl/stl/function.h"
#include "fl/stl/vector.h"
#include "fl/stl/noexcept.h"

namespace fl {
namespace audio {
namespace detector {

class Beat : public Detector {
public:
    Beat() FL_NOEXCEPT;
    ~Beat() FL_NOEXCEPT override;

    void update(shared_ptr<Context> context) override;
    void fireCallbacks() override;
    bool needsFFT() const override { return true; }
    bool needsFFTHistory() const override { return true; }
    const char* getName() const override { return "Beat"; }
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
    bool mTempoChanged = false;
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

    // Adaptive threshold (O(1) running average)
    float mAdaptiveThreshold;
    MovingAverage<float, 43> mFluxAvg;
    static constexpr size FLUX_HISTORY_SIZE = 43;  // ~1 second at 43fps

    shared_ptr<const fft::Bins> mRetainedFFT;

    float calculateSpectralFlux(const fft::Bins& fft);
    void updateAdaptiveThreshold();
    bool detectBeat(u32 timestamp);
    void updateTempo(u32 timestamp);
    void updatePhase(u32 timestamp);
};

} // namespace detector
} // namespace audio
} // namespace fl
