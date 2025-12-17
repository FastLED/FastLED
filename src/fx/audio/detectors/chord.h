#pragma once

#include "fl/audio/audio_detector.h"
#include "fl/audio/audio_context.h"
#include "fl/stl/function.h"
#include "fl/stl/shared_ptr.h"

namespace fl {

// Chord types supported by the detector
enum class ChordType {
    MAJOR,
    MINOR,
    DIMINISHED,
    AUGMENTED,
    MAJOR7,
    MINOR7,
    DOMINANT7,
    SUSPENDED2,
    SUSPENDED4,
    UNKNOWN
};

// Detected chord information
struct Chord {
    int rootNote;           // MIDI note number (0-11 for C-B)
    ChordType type;         // Chord type
    float confidence;       // Detection confidence (0.0-1.0)
    uint32_t timestamp;     // When detected

    Chord() : rootNote(-1), type(ChordType::UNKNOWN), confidence(0.0f), timestamp(0) {}
    Chord(int root, ChordType t, float conf, uint32_t ts)
        : rootNote(root), type(t), confidence(conf), timestamp(ts) {}

    bool isValid() const { return rootNote >= 0 && rootNote < 12; }

    // Declared here, defined in chord.cpp
    const char* getRootName() const;
    const char* getTypeName() const;
};

class ChordDetector : public AudioDetector {
public:
    ChordDetector();
    ~ChordDetector() override;

    void update(shared_ptr<AudioContext> context) override;
    bool needsFFT() const override { return true; }
    bool needsFFTHistory() const override { return true; }
    const char* getName() const override { return "ChordDetector"; }
    void reset() override;

    // Callbacks (multiple listeners supported)
    function_list<void(const Chord& chord)> onChord;
    function_list<void(const Chord& chord)> onChordChange;
    function_list<void()> onChordEnd;

    // State access
    const Chord& getCurrentChord() const { return mCurrentChord; }
    bool hasChord() const { return mCurrentChord.isValid(); }

    // Configuration
    void setConfidenceThreshold(float threshold) { mConfidenceThreshold = threshold; }
    void setMinDuration(uint32_t ms) { mMinChordDuration = ms; }

private:
    // Current state
    Chord mCurrentChord;
    Chord mPreviousChord;
    uint32_t mChordStartTime;
    uint32_t mChordEndTime;

    // Configuration
    float mConfidenceThreshold;
    uint32_t mMinChordDuration;

    // Chroma feature (pitch class profile)
    float mChroma[12];  // Energy for each pitch class (C, C#, D, ...)
    float mPrevChroma[12];

    // Detection methods
    void calculateChroma(const FFTBins& fft);
    Chord detectChord(const float* chroma, uint32_t timestamp);
    float matchChordPattern(const float* chroma, int root, ChordType type);
    bool isSimilarChord(const Chord& a, const Chord& b);

    // Helper methods
    void normalizeChroma(float* chroma);
    float chromaDistance(const float* a, const float* b);
};

} // namespace fl
