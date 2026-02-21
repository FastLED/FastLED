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
    void fireCallbacks() override;
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

    // State access (polling getters)
    bool isKick() const { return mKickDetected; }
    bool isSnare() const { return mSnareDetected; }
    bool isHiHat() const { return mHiHatDetected; }
    bool isTom() const { return mTomDetected; }

    // Configuration
    void setKickThreshold(float threshold) { mKickThreshold = threshold; }
    void setSnareThreshold(float threshold) { mSnareThreshold = threshold; }
    void setHiHatThreshold(float threshold) { mHiHatThreshold = threshold; }

private:
    // Per-frame detection state
    bool mKickDetected;
    bool mSnareDetected;
    bool mHiHatDetected;
    bool mTomDetected;

    float mKickThreshold;
    float mSnareThreshold;
    float mHiHatThreshold;
    float mPrevBassEnergy;
    float mPrevMidEnergy;
    float mPrevTrebleEnergy;
    u32 mLastKickTime;
    u32 mLastSnareTime;
    u32 mLastHiHatTime;

    static constexpr u32 KICK_COOLDOWN_MS = 100;
    static constexpr u32 SNARE_COOLDOWN_MS = 80;
    static constexpr u32 HIHAT_COOLDOWN_MS = 50;

    float getBassEnergy(const FFTBins& fft);
    float getMidEnergy(const FFTBins& fft);
    float getTrebleEnergy(const FFTBins& fft);
    bool detectKick(float bassEnergy, float bassFlux, u32 timestamp);
    bool detectSnare(float midEnergy, float midFlux, u32 timestamp);
    bool detectHiHat(float trebleEnergy, float trebleFlux, u32 timestamp);
};

} // namespace fl
