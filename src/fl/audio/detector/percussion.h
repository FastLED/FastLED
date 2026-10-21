#pragma once

#include "fl/audio/audio_detector.h"
#include "fl/math/filter/filter.h"
#include "fl/stl/int.h"
#include "fl/stl/function.h"
#include "fl/stl/noexcept.h"

namespace fl {
namespace audio {
namespace detector {

enum class PercussionType : u8 {
    Kick,
    Snare,
    HiHat,
    Tom,
};

class Percussion : public Detector {
public:
    Percussion() FL_NO_EXCEPT;
    ~Percussion() FL_NO_EXCEPT override;

    void update(shared_ptr<Context> context) FL_NO_EXCEPT override;
    void fireCallbacks() FL_NO_EXCEPT override;
    bool needsFFT() const FL_NO_EXCEPT override { return true; }
    bool needsFFTHistory() const FL_NO_EXCEPT override { return false; }
    const char* getName() const FL_NO_EXCEPT override { return "Percussion"; }
    void reset() FL_NO_EXCEPT override;

    // Callbacks (multiple listeners supported)
    function_list<void(PercussionType type)> onPercussionHit;
    function_list<void()> onKick;
    function_list<void()> onSnare;
    function_list<void()> onHiHat;
    function_list<void()> onTom;

    // State access (polling getters)
    bool isKick() const FL_NO_EXCEPT { return mKickDetected; }
    bool isSnare() const FL_NO_EXCEPT { return mSnareDetected; }
    bool isHiHat() const FL_NO_EXCEPT { return mHiHatDetected; }
    bool isTom() const FL_NO_EXCEPT { return mTomDetected; }

    // Confidence (0.0 - 1.0)
    float getKickConfidence() const FL_NO_EXCEPT { return mKickConfidence; }
    float getSnareConfidence() const FL_NO_EXCEPT { return mSnareConfidence; }
    float getHiHatConfidence() const FL_NO_EXCEPT { return mHiHatConfidence; }
    float getTomConfidence() const FL_NO_EXCEPT { return mTomConfidence; }

    // Feature inspection (for calibration / testing)
    float getBassToTotalRatio() const FL_NO_EXCEPT { return mBassToTotal; }
    float getTrebleToTotalRatio() const FL_NO_EXCEPT { return mTrebleToTotal; }
    float getClickRatio() const FL_NO_EXCEPT { return mClickRatio; }
    float getTrebleFlatness() const FL_NO_EXCEPT { return mTrebleFlatness; }
    float getMidToTrebleRatio() const FL_NO_EXCEPT { return mMidToTreble; }
    float getOnsetSharpness() const FL_NO_EXCEPT { return mOnsetSharpness; }
    float getSubBassProxy() const FL_NO_EXCEPT { return mSubBassProxy; }
    float getZeroCrossingFactor() const FL_NO_EXCEPT { return mZeroCrossingFactor; }

    // Configuration
    void setKickThreshold(float threshold) FL_NO_EXCEPT { mKickThreshold = threshold; }
    void setSnareThreshold(float threshold) FL_NO_EXCEPT { mSnareThreshold = threshold; }
    void setHiHatThreshold(float threshold) FL_NO_EXCEPT { mHiHatThreshold = threshold; }
    void setTomThreshold(float threshold) FL_NO_EXCEPT { mTomThreshold = threshold; }

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

    shared_ptr<const fft::Bins> mRetainedFFT;

    static constexpr u32 KICK_COOLDOWN_MS = 100;
    static constexpr u32 SNARE_COOLDOWN_MS = 80;
    static constexpr u32 HIHAT_COOLDOWN_MS = 50;
    static constexpr u32 TOM_COOLDOWN_MS = 100;

    // Feature computation
    void computeFeatures(const fft::Bins& fft) FL_NO_EXCEPT;
    void computeConfidences() FL_NO_EXCEPT;
    void applyCrossBandRejection() FL_NO_EXCEPT;
};

} // namespace detector
} // namespace audio
} // namespace fl
