#pragma once

#include "fl/vector.h"
#include "fl/audio/audio_context.h"

namespace fl {

class SilenceDetector {
public:
    // Default constants
    static constexpr float DEFAULT_SILENCE_THRESHOLD = 0.05f;  // 5% of max RMS
    static constexpr float DEFAULT_HYSTERESIS = 0.3f;          // 30% variation
    static constexpr uint32_t DEFAULT_MIN_SILENCE_MS = 100;    // 100ms
    static constexpr uint32_t DEFAULT_MAX_SILENCE_MS = 10000;  // 10s
    static constexpr size_t DEFAULT_HISTORY_SIZE = 16;         // Number of samples for smoothing

    // Constructors
    SilenceDetector();
    ~SilenceDetector();

    // Main update method
    void update(shared_ptr<AudioContext> context);
    void reset();

    // Callback types
    using VoidCallback = void(*)();
    using BoolCallback = void(*)(bool);
    using DurationCallback = void(*)(u32);

    // Silence-related callbacks
    VoidCallback onSilenceStart = nullptr;       // When silence first detected
    VoidCallback onSilenceEnd = nullptr;         // When silence ends
    BoolCallback onSilenceChange = nullptr;      // Whenever silence state changes
    DurationCallback onSilenceDuration = nullptr; // When silence reaches a duration

    // Configuration methods
    void setSilenceThreshold(float threshold) { mSilenceThreshold = threshold; }
    void setHysteresis(float hysteresis) { mHysteresis = hysteresis; }
    void setMinSilenceDuration(u32 ms) { mMinSilenceDuration = ms; }
    void setMaxSilenceDuration(u32 ms) { mMaxSilenceDuration = ms; }

    // Getters
    bool isSilent() const { return mIsSilent; }
    float getCurrentRMS() const { return mCurrentRMS; }
    u32 getSilenceDuration() const;
    u32 getSilenceStartTime() const { return mSilenceStartTime; }

private:
    // Silence state tracking
    bool mIsSilent;
    bool mPreviousSilent;
    float mCurrentRMS;

    // Detection parameters
    float mSilenceThreshold;
    float mHysteresis;

    // Timing tracking
    u32 mSilenceStartTime;
    u32 mSilenceEndTime;
    u32 mMinSilenceDuration;
    u32 mMaxSilenceDuration;
    u32 mLastUpdateTime;

    // RMS history for smoothing
    fl::vector<float> mRMSHistory;
    size_t mHistorySize;
    size_t mHistoryIndex;

    // Smoothing and threshold calculation
    float getSmoothedRMS();
    bool checkSilenceCondition(float smoothedRMS);
};

} // namespace fl
