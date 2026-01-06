#include "fl/fx/audio/detectors/note.h"
#include "fl/audio/audio_context.h"
#include "fl/stl/math.h"

namespace fl {

NoteDetector::NoteDetector()
    : mPitchDetector(fl::make_shared<PitchDetector>())
    , mOwnsPitchDetector(true)
    , mCurrentNote(NO_NOTE)
    , mLastVelocity(0)
    , mNoteActive(false)
    , mCurrentPitch(0.0f)
    , mPitchBend(0.0f)
    , mNoteOnEnergy(0.0f)
    , mNoteOnTime(0)
    , mLastUpdateTime(0)
    , mNoteOnThreshold(0.6f)
    , mNoteOffThreshold(0.4f)
    , mMinNoteDuration(50)  // 50ms minimum note duration
    , mNoteChangeThreshold(1)  // 1 semitone threshold for note change
    , mVelocitySensitivity(1.0f)
{}

NoteDetector::NoteDetector(shared_ptr<PitchDetector> pitchDetector)
    : mPitchDetector(pitchDetector)
    , mOwnsPitchDetector(false)
    , mCurrentNote(NO_NOTE)
    , mLastVelocity(0)
    , mNoteActive(false)
    , mCurrentPitch(0.0f)
    , mPitchBend(0.0f)
    , mNoteOnEnergy(0.0f)
    , mNoteOnTime(0)
    , mLastUpdateTime(0)
    , mNoteOnThreshold(0.6f)
    , mNoteOffThreshold(0.4f)
    , mMinNoteDuration(50)
    , mNoteChangeThreshold(1)
    , mVelocitySensitivity(1.0f)
{}

NoteDetector::~NoteDetector() = default;

void NoteDetector::update(shared_ptr<AudioContext> context) {
    // Update pitch detector if we own it
    if (mOwnsPitchDetector && mPitchDetector) {
        mPitchDetector->update(context);
    }

    // Get pitch detection results
    if (!mPitchDetector) {
        return;  // No pitch detector available
    }

    float pitch = mPitchDetector->getPitch();
    float confidence = mPitchDetector->getConfidence();
    bool voiced = mPitchDetector->isVoiced();
    uint32_t timestamp = context->getTimestamp();
    float energy = context->getRMS();

    mCurrentPitch = pitch;
    mLastUpdateTime = timestamp;

    // State machine: note-on, note-off, note-change detection
    if (!mNoteActive) {
        // No note currently active - check for note-on
        if (shouldTriggerNoteOn(confidence, pitch)) {
            uint8_t newNote = frequencyToMidiNote(pitch);
            uint8_t velocity = calculateVelocity(energy, confidence);

            mCurrentNote = newNote;
            mLastVelocity = velocity;
            mNoteActive = true;
            mNoteOnTime = timestamp;
            mNoteOnEnergy = energy;
            mPitchBend = calculatePitchBend(pitch, newNote);

            if (onNoteOn) {
                onNoteOn(newNote, velocity);
            }
        }
    } else {
        // Note currently active - check for note-off or note-change
        if (shouldTriggerNoteOff(confidence, voiced)) {
            // Check minimum note duration to prevent flicker
            uint32_t noteDuration = timestamp - mNoteOnTime;
            if (noteDuration >= mMinNoteDuration) {
                if (onNoteOff) {
                    onNoteOff(mCurrentNote);
                }

                mCurrentNote = NO_NOTE;
                mLastVelocity = 0;
                mNoteActive = false;
                mPitchBend = 0.0f;
            }
        } else if (voiced && confidence >= mNoteOnThreshold) {
            // Note still active - check for note change
            uint8_t newNote = frequencyToMidiNote(pitch);
            mPitchBend = calculatePitchBend(pitch, mCurrentNote);

            if (shouldTriggerNoteChange(newNote, mCurrentNote)) {
                uint8_t velocity = calculateVelocity(energy, confidence);

                // Fire note-off for old note
                if (onNoteOff) {
                    onNoteOff(mCurrentNote);
                }

                // Update to new note
                mCurrentNote = newNote;
                mLastVelocity = velocity;
                mNoteOnTime = timestamp;
                mPitchBend = calculatePitchBend(pitch, newNote);

                // Fire note-on for new note
                if (onNoteOn) {
                    onNoteOn(newNote, velocity);
                }

                // Also fire note-change callback
                if (onNoteChange) {
                    onNoteChange(newNote, velocity);
                }
            }
        }
    }
}

void NoteDetector::reset() {
    mCurrentNote = NO_NOTE;
    mLastVelocity = 0;
    mNoteActive = false;
    mCurrentPitch = 0.0f;
    mPitchBend = 0.0f;
    mNoteOnEnergy = 0.0f;
    mNoteOnTime = 0;
    mLastUpdateTime = 0;

    if (mOwnsPitchDetector && mPitchDetector) {
        mPitchDetector->reset();
    }
}

uint8_t NoteDetector::frequencyToMidiNote(float hz) const {
    if (hz <= 0.0f) {
        return NO_NOTE;
    }

    // MIDI note = 69 + 12 × log₂(f / 440)
    float semitones = 12.0f * (fl::logf(hz / A4_FREQUENCY) / fl::logf(2.0f));
    int midiNote = static_cast<int>(A4_MIDI_NOTE + semitones + 0.5f);  // Round to nearest

    // Clamp to valid MIDI range (0-127)
    if (midiNote < 0) {
        return 0;
    }
    if (midiNote > 127) {
        return 127;
    }

    return static_cast<uint8_t>(midiNote);
}

float NoteDetector::midiNoteToFrequency(uint8_t note) const {
    if (note == NO_NOTE) {
        return 0.0f;
    }

    // f = 440 × 2^((n - 69) / 12)
    float semitones = static_cast<float>(note) - A4_MIDI_NOTE;
    return A4_FREQUENCY * fl::powf(2.0f, semitones / 12.0f);
}

float NoteDetector::calculatePitchBend(float hz, uint8_t note) const {
    if (note == NO_NOTE || hz <= 0.0f) {
        return 0.0f;
    }

    float noteFrequency = midiNoteToFrequency(note);
    if (noteFrequency <= 0.0f) {
        return 0.0f;
    }

    // Calculate cents deviation from note center
    // Cents = 1200 × log₂(f / f_note)
    float cents = 1200.0f * (fl::logf(hz / noteFrequency) / fl::logf(2.0f));

    // Clamp to ±50 cents (typical range within a single note)
    return fl::clamp(cents, -50.0f, 50.0f);
}

uint8_t NoteDetector::calculateVelocity(float energy, float confidence) const {
    // Velocity calculation based on RMS energy and pitch confidence
    // Higher energy = higher velocity
    // Higher confidence = more reliable velocity

    // Normalize energy to 0-1 range (typical RMS values: 0.0-1.0)
    float normalizedEnergy = fl::clamp(energy * mVelocitySensitivity, 0.0f, 1.0f);

    // Weight by confidence (more confident = more accurate velocity)
    float weightedVelocity = normalizedEnergy * confidence;

    // Convert to MIDI velocity (1-127, 0 reserved for note-off)
    int velocity = static_cast<int>(weightedVelocity * 126.0f) + 1;

    return static_cast<uint8_t>(fl::clamp(velocity, 1, 127));
}

bool NoteDetector::shouldTriggerNoteOn(float confidence, float pitch) const {
    // Trigger note-on when:
    // 1. Confidence exceeds threshold
    // 2. Pitch is valid (non-zero)
    return (confidence >= mNoteOnThreshold) && (pitch > 0.0f);
}

bool NoteDetector::shouldTriggerNoteOff(float confidence, bool voiced) const {
    // Trigger note-off when:
    // 1. Confidence drops below off threshold (hysteresis)
    // 2. OR voice becomes unvoiced (silence or percussion)
    return (confidence < mNoteOffThreshold) || !voiced;
}

bool NoteDetector::shouldTriggerNoteChange(uint8_t newNote, uint8_t currentNote) const {
    if (newNote == NO_NOTE || currentNote == NO_NOTE) {
        return false;
    }

    // Calculate semitone difference
    int semitoneDistance = fl::fl_abs(static_cast<int>(newNote) - static_cast<int>(currentNote));

    // Trigger note change if difference exceeds threshold
    return semitoneDistance >= mNoteChangeThreshold;
}

} // namespace fl
