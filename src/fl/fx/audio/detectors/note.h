#pragma once

#include "fl/audio/audio_detector.h"
#include "fl/audio/audio_context.h"
#include "fl/fx/audio/detectors/pitch.h"
#include "fl/stl/function.h"
#include "fl/stl/shared_ptr.h"

namespace fl {

/**
 * NoteDetector - Musical note detection with MIDI output
 *
 * Converts continuous pitch (from PitchDetector) to discrete musical notes
 * using 12-tone equal temperament (12-TET). Provides note-on/note-off events
 * with velocity information based on attack strength.
 *
 * Key Features:
 * - Converts Hz to MIDI note numbers (A4 = 440 Hz = MIDI 69)
 * - Note-on/note-off events with hysteresis for stability
 * - Velocity calculation based on attack strength
 * - Configurable note-on/note-off thresholds
 * - Integration with PitchDetector for fundamental frequency tracking
 * - Support for polyphonic detection (future enhancement)
 *
 * Performance:
 * - Depends on PitchDetector (no FFT required)
 * - Update time: ~0.05-0.1ms per frame (lightweight decision logic)
 * - Memory: ~100 bytes
 */
class NoteDetector : public AudioDetector {
public:
    NoteDetector();
    explicit NoteDetector(shared_ptr<PitchDetector> pitchDetector);
    ~NoteDetector() override;

    void update(shared_ptr<AudioContext> context) override;
    bool needsFFT() const override { return false; }  // Uses pitch detection
    const char* getName() const override { return "NoteDetector"; }
    void reset() override;

    // Callbacks (multiple listeners supported)
    function_list<void(uint8_t note, uint8_t velocity)> onNoteOn;   // Note started
    function_list<void(uint8_t note)> onNoteOff;           // Note ended
    function_list<void(uint8_t note, uint8_t velocity)> onNoteChange; // Note changed while held

    // State access
    uint8_t getCurrentNote() const { return mCurrentNote; }
    uint8_t getLastVelocity() const { return mLastVelocity; }
    bool isNoteActive() const { return mNoteActive; }
    float getCurrentPitch() const { return mCurrentPitch; }
    float getPitchBend() const { return mPitchBend; }  // Cents from note center

    // Configuration
    void setNoteOnThreshold(float confidenceThreshold) { mNoteOnThreshold = confidenceThreshold; }
    void setNoteOffThreshold(float confidenceThreshold) { mNoteOffThreshold = confidenceThreshold; }
    void setMinNoteDuration(uint32_t ms) { mMinNoteDuration = ms; }
    void setNoteChangeThreshold(uint8_t semitones) { mNoteChangeThreshold = semitones; }
    void setVelocitySensitivity(float sensitivity) { mVelocitySensitivity = sensitivity; }

    // Shared pitch detector access
    void setPitchDetector(shared_ptr<PitchDetector> pitchDetector) { mPitchDetector = pitchDetector; }
    shared_ptr<PitchDetector> getPitchDetector() const { return mPitchDetector; }

private:
    // Shared pitch detector (may be shared with AudioProcessor)
    shared_ptr<PitchDetector> mPitchDetector;
    bool mOwnsPitchDetector;  // True if we created our own instance

    // Current state
    uint8_t mCurrentNote;     // Current MIDI note (0-127, 255 = none)
    uint8_t mLastVelocity;    // Velocity of last note-on event (0-127)
    bool mNoteActive;         // True if note is currently held
    float mCurrentPitch;      // Current pitch in Hz
    float mPitchBend;         // Cents deviation from note center (-50 to +50)
    float mNoteOnEnergy;      // Energy level at note onset

    // Timing
    uint32_t mNoteOnTime;     // Timestamp of note-on event
    uint32_t mLastUpdateTime; // Timestamp of last update

    // Configuration
    float mNoteOnThreshold;   // Confidence threshold for note-on (default: 0.6)
    float mNoteOffThreshold;  // Confidence threshold for note-off (default: 0.4)
    uint32_t mMinNoteDuration;// Minimum note duration to prevent flicker (ms)
    uint8_t mNoteChangeThreshold; // Semitone threshold for note change event
    float mVelocitySensitivity;   // Velocity calculation sensitivity

    // Helper methods
    uint8_t frequencyToMidiNote(float hz) const;
    float midiNoteToFrequency(uint8_t note) const;
    float calculatePitchBend(float hz, uint8_t note) const;
    uint8_t calculateVelocity(float energy, float confidence) const;
    bool shouldTriggerNoteOn(float confidence, float pitch) const;
    bool shouldTriggerNoteOff(float confidence, bool voiced) const;
    bool shouldTriggerNoteChange(uint8_t newNote, uint8_t currentNote) const;

    // Constants
    static constexpr float A4_FREQUENCY = 440.0f;  // A4 reference pitch
    static constexpr uint8_t A4_MIDI_NOTE = 69;    // A4 MIDI note number
    static constexpr uint8_t NO_NOTE = 255;        // Sentinel for no active note
};

} // namespace fl
