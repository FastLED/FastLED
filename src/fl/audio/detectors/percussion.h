#pragma once

#include "fl/audio/audio_detector.h"
#include "fl/filter.h"
#include "fl/stl/int.h"
#include "fl/stl/function.h"

namespace fl {

enum class PercussionType : u8 {
    Kick,
    Snare,
    HiHat,
    Tom,
};

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
    function_list<void(PercussionType type)> onPercussionHit;
    function_list<void()> onKick;
    function_list<void()> onSnare;
    function_list<void()> onHiHat;
    function_list<void()> onTom;

    // State access (polling getters)
    bool isKick() const { return mKickDetected; }
    bool isSnare() const { return mSnareDetected; }
    bool isHiHat() const { return mHiHatDetected; }
    bool isTom() const { return mTomDetected; }

    // Confidence (0.0 - 1.0)
    float getKickConfidence() const { return mKickConfidence; }
    float getSnareConfidence() const { return mSnareConfidence; }
    float getHiHatConfidence() const { return mHiHatConfidence; }
    float getTomConfidence() const { return mTomConfidence; }

    // Feature inspection (for calibration / testing)
    float getBassToTotalRatio() const { return mBassToTotal; }
    float getTrebleToTotalRatio() const { return mTrebleToTotal; }
    float getClickRatio() const { return mClickRatio; }
    float getTrebleFlatness() const { return mTrebleFlatness; }
    float getMidToTrebleRatio() const { return mMidToTreble; }
    float getOnsetSharpness() const { return mOnsetSharpness; }
    float getSubBassProxy() const { return mSubBassProxy; }
    float getZeroCrossingFactor() const { return mZeroCrossingFactor; }

    // Configuration
    void setKickThreshold(float threshold) { mKickThreshold = threshold; }
    void setSnareThreshold(float threshold) { mSnareThreshold = threshold; }
    void setHiHatThreshold(float threshold) { mHiHatThreshold = threshold; }
    void setTomThreshold(float threshold) { mTomThreshold = threshold; }

private:
    // Per-frame detection state
    bool mKickDetected;
    bool mSnareDetected;
    bool mHiHatDetected;
    bool mTomDetected;

    // Confidence scores (0.0 - 1.0)
    float mKickConfidence;
    float mSnareConfidence;
    float mHiHatConfidence;
    float mTomConfidence;

    // Spectral features (computed per frame)
    float mBassToTotal;
    float mTrebleToTotal;
    float mClickRatio;
    float mTrebleFlatness;
    float mMidToTreble;
    float mOnsetSharpness;
    float mSubBassProxy;
    float mZeroCrossingFactor;

    // Thresholds
    float mKickThreshold;
    float mSnareThreshold;
    float mHiHatThreshold;
    float mTomThreshold;

    // Envelope followers for onset detection
    AttackDecayFilter<float> mTotalEnvelope{0.15f, 0.005f};

    // Cooldown timestamps
    u32 mLastKickTime;
    u32 mLastSnareTime;
    u32 mLastHiHatTime;
    u32 mLastTomTime;

    shared_ptr<const FFTBins> mRetainedFFT;

    static constexpr u32 KICK_COOLDOWN_MS = 100;
    static constexpr u32 SNARE_COOLDOWN_MS = 80;
    static constexpr u32 HIHAT_COOLDOWN_MS = 50;
    static constexpr u32 TOM_COOLDOWN_MS = 100;

    // Feature computation
    void computeFeatures(const FFTBins& fft);
    void computeConfidences();
    void applyCrossBandRejection();
};

} // namespace fl
