#pragma once

#include "fl/audio/audio_context.h"
#include "fl/stl/function.h"

namespace fl {

class PercussionDetector {
public:
    PercussionDetector();
    ~PercussionDetector();

    void update(shared_ptr<AudioContext> context);
    void reset();

    // Cooldown periods to prevent double-triggering (in milliseconds)
    static constexpr u32 KICK_COOLDOWN_MS = 50;
    static constexpr u32 SNARE_COOLDOWN_MS = 50;
    static constexpr u32 HIHAT_COOLDOWN_MS = 30;

    // Percussion detection callbacks (multiple listeners supported)
    function_list<void()> onKick;
    function_list<void()> onSnare;
    function_list<void()> onHiHat;
    function_list<void()> onTom;
    function_list<void(const char* type)> onPercussionHit;

    // Configuration methods
    void setKickThreshold(float threshold) { mKickThreshold = threshold; }
    void setSnareThreshold(float threshold) { mSnareThreshold = threshold; }
    void setHiHatThreshold(float threshold) { mHiHatThreshold = threshold; }

private:
    // Detection thresholds
    float mKickThreshold;
    float mSnareThreshold;
    float mHiHatThreshold;

    // Previous energy tracking
    float mPrevBassEnergy;
    float mPrevMidEnergy;
    float mPrevTrebleEnergy;

    // Timestamp tracking to prevent double-triggering
    u32 mLastKickTime;
    u32 mLastSnareTime;
    u32 mLastHiHatTime;

    // Energy band calculations
    float getBassEnergy(const FFTBins& fft);
    float getMidEnergy(const FFTBins& fft);
    float getTrebleEnergy(const FFTBins& fft);

    // Percussion type detection
    bool detectKick(float bassEnergy, float bassFlux, u32 timestamp);
    bool detectSnare(float midEnergy, float midFlux, u32 timestamp);
    bool detectHiHat(float trebleEnergy, float trebleFlux, u32 timestamp);
};

} // namespace fl
