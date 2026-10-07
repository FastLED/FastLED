#pragma once

#include "fl/audio/audio_detector.h"
#include "fl/stl/function.h"
#include "fl/stl/vector.h"
#include "fl/stl/noexcept.h"

namespace fl {
namespace audio {
namespace detector {

class Silence : public Detector {
public:
    Silence() FL_NO_EXCEPT;
    ~Silence() FL_NO_EXCEPT override;

    void update(shared_ptr<Context> context) FL_NO_EXCEPT override;
    void fireCallbacks() FL_NO_EXCEPT override;
    bool needsFFT() const FL_NO_EXCEPT override { return false; }  // Uses RMS from Sample
    const char* getName() const FL_NO_EXCEPT override { return "Silence"; }
    void reset() FL_NO_EXCEPT override;

    // Callbacks (multiple listeners supported)
    function_list<void(u8 silent)> onSilence;
    function_list<void()> onSilenceStart;
    function_list<void()> onSilenceEnd;
    function_list<void(u32 durationMs)> onSilenceDuration;

    // State access
    bool isSilent() const FL_NO_EXCEPT { return mIsSilent; }
    u32 getSilenceDuration() const FL_NO_EXCEPT;
    float getSilenceThreshold() const FL_NO_EXCEPT { return mSilenceThreshold; }
    float getCurrentRMS() const FL_NO_EXCEPT { return mCurrentRMS; }

    // Configuration
    void setSilenceThreshold(float threshold) FL_NO_EXCEPT { mSilenceThreshold = threshold; }
    void setMinSilenceDuration(u32 durationMs) FL_NO_EXCEPT { mMinSilenceDuration = durationMs; }
    void setMaxSilenceDuration(u32 durationMs) FL_NO_EXCEPT { mMaxSilenceDuration = durationMs; }
    void setHysteresis(float hysteresis) FL_NO_EXCEPT { mHysteresis = hysteresis; }

private:
    bool mIsSilent;
    bool mPreviousSilent;
    float mCurrentRMS;
    float mSilenceThreshold;
    float mHysteresis;

    u32 mSilenceStartTime;
    u32 mSilenceEndTime;
    u32 mMinSilenceDuration;
    u32 mMaxSilenceDuration;
    u32 mLastUpdateTime;

    // History for smoothing
    vector<float> mRMSHistory;
    int mHistorySize;
    int mHistoryIndex;

    enum class PendingSilenceEvent : u8 { kNone, kStart, kEnd, kMaxDuration };
    PendingSilenceEvent mPendingEvent = PendingSilenceEvent::kNone;
    u32 mPendingDuration = 0;

    static constexpr u32 DEFAULT_MIN_SILENCE_MS = 500;
    static constexpr u32 DEFAULT_MAX_SILENCE_MS = 60000;  // 1 minute
    static constexpr float DEFAULT_SILENCE_THRESHOLD = 0.01f;
    static constexpr float DEFAULT_HYSTERESIS = 0.2f;
    static constexpr int DEFAULT_HISTORY_SIZE = 5;

    float getSmoothedRMS() FL_NO_EXCEPT;
    bool checkSilenceCondition(float smoothedRMS) FL_NO_EXCEPT;
};

} // namespace detector
} // namespace audio
} // namespace fl
