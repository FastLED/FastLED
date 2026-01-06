// KeyDetector - Musical key detection using chroma features and key profiles
//
// Detects the musical key (tonal center) of the audio using Krumhansl-Schmuckler
// key-finding algorithm. Analyzes pitch class profiles (chroma) and correlates
// them with theoretical key profiles for major and minor keys.
//
// Features:
// - Detects 24 possible keys (12 major + 12 minor)
// - Uses chroma features from FFT analysis
// - Krumhansl-Schmuckler key profiles for accurate detection
// - Temporal averaging for stable key estimation
// - Confidence scoring based on correlation strength
// - Event callbacks for key changes
//
// Usage:
//   KeyDetector detector;
//   detector.onKeyChange([](const Key& key) {
//       Serial.print("Key: ");
//       Serial.print(key.getKeyName());
//       Serial.print(" (confidence: ");
//       Serial.print(key.confidence);
//       Serial.println(")");
//   });
//
//   void loop() {
//       AudioSample sample = audio.next();
//       shared_ptr<AudioContext> context = make_shared<AudioContext>(sample);
//       detector.update(context);
//   }

#pragma once

#include "fl/audio/audio_detector.h"
#include "fl/audio/audio_context.h"
#include "fl/stl/function.h"
#include "fl/stl/vector.h"
#include "fl/int.h"

namespace fl {

// Forward declaration
class AudioContext;

// Key structure representing detected musical key
struct Key {
    u8 rootNote;             // 0-11 (C=0, C#=1, D=2, ..., B=11)
    bool isMinor;            // true = minor key, false = major key
    float confidence;        // 0.0-1.0 correlation with key profile
    u32 timestamp;           // Detection timestamp (ms)
    u32 duration;            // How long this key has been active (ms)

    Key() : rootNote(0), isMinor(false), confidence(0.0f), timestamp(0), duration(0) {}

    Key(u8 root, bool minor, float conf, u32 time)
        : rootNote(root), isMinor(minor), confidence(conf), timestamp(time), duration(0) {}

    // Get key name (e.g., "C", "F#", "Bb")
    const char* getRootName() const;

    // Get quality name ("maj" or "min")
    const char* getQuality() const { return isMinor ? "min" : "maj"; }

    // Get full key name (e.g., "C maj", "F# min")
    void getKeyName(char* buffer, size_t bufferSize) const;

    // Check if key is valid
    bool isValid() const { return confidence > 0.0f; }

    // Compare keys
    bool operator==(const Key& other) const {
        return rootNote == other.rootNote && isMinor == other.isMinor;
    }

    bool operator!=(const Key& other) const {
        return !(*this == other);
    }
};

// KeyDetector - Detects musical key using chroma analysis
class KeyDetector : public AudioDetector {
public:
    KeyDetector();
    ~KeyDetector() override;

    // AudioDetector interface
    void update(shared_ptr<AudioContext> context) override;
    bool needsFFT() const override { return true; }
    const char* getName() const override { return "KeyDetector"; }
    void reset() override;

    // Event callbacks (multiple listeners supported)
    function_list<void(const Key& key)> onKey;           // Every frame with current key
    function_list<void(const Key& key)> onKeyChange;     // When key changes
    function_list<void()> onKeyEnd;                  // When key ends (confidence drops)

    // State access
    const Key& getCurrentKey() const { return mCurrentKey; }
    bool hasKey() const { return mCurrentKey.isValid(); }

    // Configuration
    void setConfidenceThreshold(float threshold) { mConfidenceThreshold = threshold; }
    void setMinDuration(u32 ms) { mMinKeyDuration = ms; }
    void setAveragingFrames(int frames) { mAveragingFrames = frames; }

private:
    // Current state
    Key mCurrentKey;
    Key mPreviousKey;
    u32 mKeyStartTime;
    bool mKeyActive;

    // Configuration
    float mConfidenceThreshold;    // Minimum confidence to detect key (default: 0.65)
    u32 mMinKeyDuration;           // Minimum duration for stable key (default: 2000ms)
    int mAveragingFrames;          // Number of frames to average (default: 8)

    // Chroma history for temporal averaging
    vector<float> mChromaHistory[12];  // History for each pitch class
    int mHistoryIndex;
    int mHistorySize;

    // Key profiles (Krumhansl-Schmuckler)
    static const float MAJOR_PROFILE[12];
    static const float MINOR_PROFILE[12];

    // Helper methods
    void extractChroma(const FFTBins& fft, float* chroma);
    void normalizeChroma(float* chroma);
    void updateChromaHistory(const float* chroma);
    void getAveragedChroma(float* chroma);
    Key detectKey(const float* chroma, u32 timestamp);
    float correlateWithProfile(const float* chroma, const float* profile, int rootNote);
    void fireCallbacks(const Key& key, u32 timestamp);
};

} // namespace fl
