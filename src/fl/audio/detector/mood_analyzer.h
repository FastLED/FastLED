// MoodAnalyzer - Mood and emotion detection from audio features
// Part of FastLED Audio Processing System (Phase 3 - Tier 3)

#pragma once

#include "fl/audio/audio_detector.h"
#include "fl/stl/function.h"
#include "fl/stl/int.h"
#include "fl/stl/noexcept.h"

namespace fl {
namespace audio {
namespace detector {

// Mood state representing emotional characteristics
struct Mood {
    float valence;      // Positivity/negativity (-1.0 to 1.0, negative=sad, positive=happy)
    float arousal;      // Energy/calmness (0.0 to 1.0, low=calm, high=energetic)
    float confidence;   // Detection confidence (0.0 to 1.0)
    u32 timestamp;      // Timestamp of mood detection
    u32 duration;       // Duration mood has been stable (ms)

    // Mood categories (derived from valence/arousal)
    enum Category {
        CALM_NEGATIVE,      // Low arousal, negative valence (sad, melancholic)
        CALM_POSITIVE,      // Low arousal, positive valence (peaceful, content)
        ENERGETIC_NEGATIVE, // High arousal, negative valence (angry, tense)
        ENERGETIC_POSITIVE, // High arousal, positive valence (happy, excited)
        NEUTRAL             // Near center, no strong mood
    };

    Mood() FL_NO_EXCEPT : valence(0.0f), arousal(0.0f), confidence(0.0f), timestamp(0), duration(0) {}

    bool isValid() const FL_NO_EXCEPT { return confidence > 0.0f; }

    Category getCategory() const FL_NO_EXCEPT {
        const float NEUTRAL_THRESHOLD = 0.3f;

        // Near center = neutral
        if (abs(valence) < NEUTRAL_THRESHOLD && arousal < NEUTRAL_THRESHOLD + 0.2f) {
            return NEUTRAL;
        }

        // Quadrant-based categorization
        if (arousal < 0.5f) {
            return valence < 0.0f ? CALM_NEGATIVE : CALM_POSITIVE;
        } else {
            return valence < 0.0f ? ENERGETIC_NEGATIVE : ENERGETIC_POSITIVE;
        }
    }

    const char* getCategoryName() const FL_NO_EXCEPT {
        switch (getCategory()) {
            case CALM_NEGATIVE:      return "calm_negative";
            case CALM_POSITIVE:      return "calm_positive";
            case ENERGETIC_NEGATIVE: return "energetic_negative";
            case ENERGETIC_POSITIVE: return "energetic_positive";
            case NEUTRAL:            return "neutral";
            default:                 return "unknown";
        }
    }
};

class MoodAnalyzer : public Detector {
public:
    MoodAnalyzer() FL_NO_EXCEPT;
    ~MoodAnalyzer() FL_NO_EXCEPT override;

    // Detector interface
    void update(shared_ptr<Context> context) FL_NO_EXCEPT override;
    void fireCallbacks() FL_NO_EXCEPT override;
    bool needsFFT() const FL_NO_EXCEPT override { return true; }
    bool needsFFTHistory() const FL_NO_EXCEPT override { return true; }
    const char* getName() const FL_NO_EXCEPT override { return "MoodAnalyzer"; }
    void reset() FL_NO_EXCEPT override;

    // Event callbacks (multiple listeners supported)
    function_list<void(const Mood& mood)> onMood;              // Every frame
    function_list<void(const Mood& mood)> onMoodChange;        // Mood category changes
    function_list<void(float valence, float arousal)> onValenceArousal;   // Raw values

    // State access
    const Mood& getCurrentMood() const FL_NO_EXCEPT { return mCurrentMood; }
    float getValence() const FL_NO_EXCEPT { return mCurrentMood.valence; }
    float getArousal() const FL_NO_EXCEPT { return mCurrentMood.arousal; }
    Mood::Category getMoodCategory() const FL_NO_EXCEPT { return mCurrentMood.getCategory(); }

    // Configuration
    void setConfidenceThreshold(float threshold) FL_NO_EXCEPT { mConfidenceThreshold = threshold; }
    void setMinDuration(u32 ms) FL_NO_EXCEPT { mMinDuration = ms; }
    void setAveragingFrames(int frames) FL_NO_EXCEPT { mAveragingFrames = frames; }

private:
    Mood mCurrentMood;
    Mood mPreviousMood;
    bool mMoodChanged = false;
    float mConfidenceThreshold;
    u32 mMinDuration;
    int mAveragingFrames;

    // Audio features for mood analysis
    float mSpectralCentroid;
    float mSpectralRolloff;
    float mSpectralFlux;
    float mZeroCrossingRate;
    float mRMSEnergy;

    // Feature history for temporal averaging
    fl::vector<float> mValenceHistory;
    fl::vector<float> mArousalHistory;
    int mHistoryIndex;

    shared_ptr<const fft::Bins> mRetainedFFT;

    // Analysis methods
    float calculateSpectralCentroid(const fft::Bins& fft) FL_NO_EXCEPT;
    float calculateSpectralRolloff(const fft::Bins& fft, float threshold = 0.85f) FL_NO_EXCEPT;
    float calculateSpectralFlux(const fft::Bins& fft, const fft::Bins* prevFFT) FL_NO_EXCEPT;
    float calculateValence(float centroid, float rolloff, float flux) FL_NO_EXCEPT;
    float calculateArousal(float rms, float zcr, float flux) FL_NO_EXCEPT;
    float calculateConfidence(float valence, float arousal) FL_NO_EXCEPT;
    bool shouldChangeMood(const Mood& newMood) FL_NO_EXCEPT;
};

} // namespace detector
} // namespace audio
} // namespace fl
