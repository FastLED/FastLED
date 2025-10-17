#pragma once

#include "fl/audio/audio_context.h"
#include "fl/math.h"

namespace fl {

// Enumerates different chord types
enum class ChordType {
    UNKNOWN = 0,
    MAJOR,
    MINOR,
    DIMINISHED,
    AUGMENTED,
    MAJOR7,
    MINOR7,
    DOMINANT7,
    SUSPENDED2,
    SUSPENDED4
};

// Represents a detected musical chord
struct Chord {
    uint8_t rootNote;       // Root note of the chord (0-11)
    ChordType type;         // Type of chord
    float confidence;       // Confidence of chord detection
    uint32_t timestamp;     // When the chord was detected

    // Constructors
    Chord()
        : rootNote(0), type(ChordType::UNKNOWN), confidence(0.0f), timestamp(0) {}

    Chord(uint8_t root, ChordType chordType, float conf, uint32_t ts)
        : rootNote(root), type(chordType), confidence(conf), timestamp(ts) {}

    // Note name helpers
    const char* getRootName() const;
    const char* getTypeName() const;

    // Validity check
    bool isValid() const {
        return type != ChordType::UNKNOWN && confidence > 0.0f;
    }

    // Comparison
    bool operator==(const Chord& other) const {
        return rootNote == other.rootNote && type == other.type;
    }
};

class ChordDetector {
public:
    ChordDetector();
    ~ChordDetector();

    void update(shared_ptr<AudioContext> context);
    void reset();

    // Callback types
    using ChordCallback = void(*)(const Chord& chord);
    using VoidCallback = void(*)();

    // Chord-related callbacks
    ChordCallback onChordChange = nullptr;    // When chord changes
    ChordCallback onChord = nullptr;          // Continuously when chord is active
    VoidCallback onChordEnd = nullptr;        // When chord ends

    // Configuration methods
    void setConfidenceThreshold(float threshold) { mConfidenceThreshold = threshold; }
    void setMinChordDuration(uint32_t ms) { mMinChordDuration = ms; }

    // Getters
    const Chord& getCurrentChord() const { return mCurrentChord; }
    const Chord& getPreviousChord() const { return mPreviousChord; }

private:
    // Chroma representation (12-tone equal temperament)
    float mChroma[12];
    float mPrevChroma[12];

    // Chord detection state
    Chord mCurrentChord;
    Chord mPreviousChord;
    uint32_t mChordStartTime;
    uint32_t mChordEndTime;

    // Detection parameters
    float mConfidenceThreshold;
    uint32_t mMinChordDuration;

    // Chroma and chord detection methods
    void calculateChroma(const FFTBins& fft);
    Chord detectChord(const float* chroma, uint32_t timestamp);
    float matchChordPattern(const float* chroma, int root, ChordType type);
    void normalizeChroma(float* chroma);
    float chromaDistance(const float* a, const float* b);

    // Helpers
    bool isSimilarChord(const Chord& a, const Chord& b);
};

} // namespace fl