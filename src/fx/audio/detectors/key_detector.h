#pragma once

#include "fl/vector.h"
#include "fl/audio/audio_context.h"

namespace fl {

// Forward declaration of Key struct
struct Key {
    u8 rootNote;     // 0-11 (C, C#, D, etc.)
    bool isMinor;    // Major or minor
    float confidence; // 0-1 confidence of key detection
    u32 duration;    // Time key has been active (ms)

    Key() : rootNote(0), isMinor(false), confidence(0.0f), duration(0) {}
    Key(u8 root, bool minor, float conf, u32 timestamp)
        : rootNote(root), isMinor(minor), confidence(conf), duration(0) {}

    const char* getRootName() const;
    void getKeyName(char* buffer, size_t bufferSize) const;
    const char* getQuality() const { return isMinor ? "min" : "maj"; }

    bool operator==(const Key& other) const {
        return rootNote == other.rootNote && isMinor == other.isMinor;
    }

    bool operator!=(const Key& other) const {
        return !(*this == other);
    }
};

class KeyDetector {
public:
    KeyDetector();
    ~KeyDetector();

    void update(shared_ptr<AudioContext> context);
    void reset();

    // Callback function types
    using KeyCallback = void(*)(const Key& key);
    using VoidCallback = void(*)();

    // Callbacks for key detection events
    KeyCallback onKeyChange = nullptr;  // When key changes
    KeyCallback onKey = nullptr;        // Every frame when key is active
    VoidCallback onKeyEnd = nullptr;    // When key is no longer detected

private:
    // Static key profiles for major and minor keys
    static const float MAJOR_PROFILE[12];
    static const float MINOR_PROFILE[12];

    // Chroma extraction and processing methods
    void extractChroma(const FFTBins& fft, float* chroma);
    void normalizeChroma(float* chroma);
    void updateChromaHistory(const float* chroma);
    void getAveragedChroma(float* chroma);

    // Key detection algorithm
    Key detectKey(const float* chroma, u32 timestamp);
    float correlateWithProfile(const float* chroma, const float* profile, int rootNote);
    void fireCallbacks(const Key& key, u32 timestamp);

    // Key detection state
    Key mCurrentKey;
    Key mPreviousKey;
    u32 mKeyStartTime;
    bool mKeyActive;

    // Configuration parameters
    float mConfidenceThreshold;
    u32 mMinKeyDuration;
    int mAveragingFrames;

    // Chroma history for temporal smoothing
    fl::vector<float> mChromaHistory[12];
    int mHistoryIndex;
    int mHistorySize;
};

} // namespace fl
