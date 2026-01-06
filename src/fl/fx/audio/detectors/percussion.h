#pragma once

#include "fl/audio/audio_detector.h"
#include "fl/audio/audio_context.h"
#include "fl/stl/function.h"

namespace fl {

class PercussionDetector : public AudioDetector {
public:
    PercussionDetector();
    ~PercussionDetector() override;

    void update(shared_ptr<AudioContext> context) override;
    bool needsFFT() const override { return true; }
    bool needsFFTHistory() const override { return false; }
    const char* getName() const override { return "PercussionDetector"; }
    void reset() override;

    // Callbacks (multiple listeners supported)
    function_list<void(const char* type)> onPercussionHit;
    function_list<void()> onKick;
    function_list<void()> onSnare;
    function_list<void()> onHiHat;
    function_list<void()> onTom;

    // Configuration
    void setKickThreshold(float threshold) { mKickThreshold = threshold; }
    void setSnareThreshold(float threshold) { mSnareThreshold = threshold; }
    void setHiHatThreshold(float threshold) { mHiHatThreshold = threshold; }

private:
    float mKickThreshold;
    float mSnareThreshold;
    float mHiHatThreshold;
    float mPrevBassEnergy;
    float mPrevMidEnergy;
    float mPrevTrebleEnergy;
    uint32_t mLastKickTime;
    uint32_t mLastSnareTime;
    uint32_t mLastHiHatTime;

    static constexpr uint32_t KICK_COOLDOWN_MS = 100;
    static constexpr uint32_t SNARE_COOLDOWN_MS = 80;
    static constexpr uint32_t HIHAT_COOLDOWN_MS = 50;

    float getBassEnergy(const FFTBins& fft);
    float getMidEnergy(const FFTBins& fft);
    float getTrebleEnergy(const FFTBins& fft);
    bool detectKick(float bassEnergy, float bassFlux, uint32_t timestamp);
    bool detectSnare(float midEnergy, float midFlux, uint32_t timestamp);
    bool detectHiHat(float trebleEnergy, float trebleFlux, uint32_t timestamp);
};

} // namespace fl
