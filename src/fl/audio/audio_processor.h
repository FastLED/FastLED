#pragma once

#ifndef FL_AUDIO_AUDIO_PROCESSOR_H
#define FL_AUDIO_AUDIO_PROCESSOR_H

#include "fl/audio/audio.h"  // IWYU pragma: keep
#include "fl/audio/audio_context.h"  // IWYU pragma: keep
#include "fl/audio/audio_detector.h"  // IWYU pragma: keep
#include "fl/audio/detector/vibe.h"  // IWYU pragma: keep - detector::VibeLevels used in public callback API
#include "fl/audio/mic_profiles.h"
#include "fl/audio/signal_conditioner.h"
#include "fl/audio/noise_floor_tracker.h"
#include "fl/stl/shared_ptr.h"
#include "fl/stl/function.h"  // IWYU pragma: keep
#include "fl/stl/vector.h"
#include "fl/task/task.h"
#include "fl/stl/noexcept.h"

namespace fl {
namespace audio {

class AudioManager;
class IInput;

// Forward declarations of detector types (defined in fl::audio::detector)
namespace detector {
class Beat;
class FrequencyBands;
class EnergyAnalyzer;
class TempoAnalyzer;
class Transient;
class Silence;
class DynamicsAnalyzer;
class Pitch;
class Note;
class Downbeat;
class Backbeat;
class Vocal;
class Percussion;
enum class PercussionType : u8;
class ChordDetector;
struct Chord;
class KeyDetector;
struct Key;
class MoodAnalyzer;
struct Mood;
class BuildupDetector;
struct Buildup;
class DropDetector;
struct Drop;
class EqualizerDetector;
struct Equalizer;
struct EqualizerConfig;
class Vibe;
} // namespace detector

class Processor {
public:
    Processor() FL_NOEXCEPT;
    ~Processor() FL_NOEXCEPT;

    // ----- Main Update -----
    void update(const Sample& sample) FL_NOEXCEPT;

    // Update detectors using an externally-provided Context (FFT already cached).
    // Skips signal conditioning and setSample — caller is responsible for those.
    void updateFromContext(shared_ptr<Context> externalContext) FL_NOEXCEPT;

    // ----- Beat Detection Events -----
    void onBeat(function<void()> callback) FL_NOEXCEPT;
    void onBeatPhase(function<void(float phase)> callback) FL_NOEXCEPT;
    void onOnset(function<void(float strength)> callback) FL_NOEXCEPT;
    void onTempoChange(function<void(float bpm, float confidence)> callback) FL_NOEXCEPT;

    // ----- Tempo Analysis Events -----
    void onTempo(function<void(float bpm)> callback) FL_NOEXCEPT;
    void onTempoWithConfidence(function<void(float bpm, float confidence)> callback) FL_NOEXCEPT;
    void onTempoStable(function<void()> callback) FL_NOEXCEPT;
    void onTempoUnstable(function<void()> callback) FL_NOEXCEPT;

    // ----- Frequency Band Events -----
    void onBass(function<void(float level)> callback) FL_NOEXCEPT;
    void onMid(function<void(float level)> callback) FL_NOEXCEPT;
    void onTreble(function<void(float level)> callback) FL_NOEXCEPT;
    void onFrequencyBands(function<void(float bass, float mid, float treble)> callback) FL_NOEXCEPT;

    // ----- detector::Equalizer Events (WLED-style, all 0.0-1.0) -----
    void onEqualizer(function<void(const detector::Equalizer&)> callback) FL_NOEXCEPT;

    // ----- Energy/Level Events -----
    void onEnergy(function<void(float rms)> callback) FL_NOEXCEPT;
    void onNormalizedEnergy(function<void(float normalizedRms)> callback) FL_NOEXCEPT;
    void onPeak(function<void(float peak)> callback) FL_NOEXCEPT;
    void onAverageEnergy(function<void(float avgEnergy)> callback) FL_NOEXCEPT;

    // ----- Transient Detection Events -----
    void onTransient(function<void()> callback) FL_NOEXCEPT;
    void onTransientWithStrength(function<void(float strength)> callback) FL_NOEXCEPT;
    void onAttack(function<void(float strength)> callback) FL_NOEXCEPT;

    // ----- Silence Detection Events -----
    void onSilence(function<void(u8 silent)> callback) FL_NOEXCEPT;
    void onSilenceStart(function<void()> callback) FL_NOEXCEPT;
    void onSilenceEnd(function<void()> callback) FL_NOEXCEPT;
    void onSilenceDuration(function<void(u32 durationMs)> callback) FL_NOEXCEPT;

    // ----- Dynamics Analysis Events -----
    void onCrescendo(function<void()> callback) FL_NOEXCEPT;
    void onDiminuendo(function<void()> callback) FL_NOEXCEPT;
    void onDynamicTrend(function<void(float trend)> callback) FL_NOEXCEPT;
    void onCompressionRatio(function<void(float compression)> callback) FL_NOEXCEPT;

    // ----- Pitch Detection Events -----
    void onPitch(function<void(float hz)> callback) FL_NOEXCEPT;
    void onPitchWithConfidence(function<void(float hz, float confidence)> callback) FL_NOEXCEPT;
    void onPitchChange(function<void(float hz)> callback) FL_NOEXCEPT;
    void onVoiced(function<void(u8 voiced)> callback) FL_NOEXCEPT;

    // ----- Note Detection Events -----
    void onNoteOn(function<void(u8 note, u8 velocity)> callback) FL_NOEXCEPT;
    void onNoteOff(function<void(u8 note)> callback) FL_NOEXCEPT;
    void onNoteChange(function<void(u8 note, u8 velocity)> callback) FL_NOEXCEPT;

    // ----- Downbeat Detection Events -----
    void onDownbeat(function<void()> callback) FL_NOEXCEPT;
    void onMeasureBeat(function<void(u8 beatNumber)> callback) FL_NOEXCEPT;
    void onMeterChange(function<void(u8 beatsPerMeasure)> callback) FL_NOEXCEPT;
    void onMeasurePhase(function<void(float phase)> callback) FL_NOEXCEPT;

    // ----- Backbeat Detection Events -----
    void onBackbeat(function<void(u8 beatNumber, float confidence, float strength)> callback) FL_NOEXCEPT;

    // ----- Vocal Detection Events -----
    void onVocal(function<void(u8 active)> callback) FL_NOEXCEPT;
    void onVocalStart(function<void()> callback) FL_NOEXCEPT;
    void onVocalEnd(function<void()> callback) FL_NOEXCEPT;
    void onVocalConfidence(function<void(float confidence)> callback) FL_NOEXCEPT;

    // ----- Percussion Detection Events -----
    void onPercussion(function<void(detector::PercussionType type)> callback) FL_NOEXCEPT;
    void onKick(function<void()> callback) FL_NOEXCEPT;
    void onSnare(function<void()> callback) FL_NOEXCEPT;
    void onHiHat(function<void()> callback) FL_NOEXCEPT;
    void onTom(function<void()> callback) FL_NOEXCEPT;

    // ----- detector::Chord Detection Events -----
    void onChord(function<void(const detector::Chord& chord)> callback) FL_NOEXCEPT;
    void onChordChange(function<void(const detector::Chord& chord)> callback) FL_NOEXCEPT;
    void onChordEnd(function<void()> callback) FL_NOEXCEPT;

    // ----- detector::Key Detection Events -----
    void onKey(function<void(const detector::Key& key)> callback) FL_NOEXCEPT;
    void onKeyChange(function<void(const detector::Key& key)> callback) FL_NOEXCEPT;
    void onKeyEnd(function<void()> callback) FL_NOEXCEPT;

    // ----- detector::Mood Analysis Events -----
    void onMood(function<void(const detector::Mood& mood)> callback) FL_NOEXCEPT;
    void onMoodChange(function<void(const detector::Mood& mood)> callback) FL_NOEXCEPT;
    void onValenceArousal(function<void(float valence, float arousal)> callback) FL_NOEXCEPT;

    // ----- detector::Buildup Detection Events -----
    void onBuildupStart(function<void()> callback) FL_NOEXCEPT;
    void onBuildupProgress(function<void(float progress)> callback) FL_NOEXCEPT;
    void onBuildupPeak(function<void()> callback) FL_NOEXCEPT;
    void onBuildupEnd(function<void()> callback) FL_NOEXCEPT;
    void onBuildup(function<void(const detector::Buildup&)> callback) FL_NOEXCEPT;

    // ----- detector::Drop Detection Events -----
    void onDrop(function<void()> callback) FL_NOEXCEPT;
    void onDropEvent(function<void(const detector::Drop&)> callback) FL_NOEXCEPT;
    void onDropImpact(function<void(float impact)> callback) FL_NOEXCEPT;

    // ----- Vibe Audio-Reactive Events -----
    // Comprehensive self-normalizing levels, spikes, and raw values
    void onVibeLevels(function<void(const detector::VibeLevels&)> callback) FL_NOEXCEPT;
    void onVibeBassSpike(function<void()> callback) FL_NOEXCEPT;
    void onVibeMidSpike(function<void()> callback) FL_NOEXCEPT;
    void onVibeTrebSpike(function<void()> callback) FL_NOEXCEPT;

    // ----- Polling Getters (float 0.0-1.0, bool, or integer) -----

    // Vocal Detection
    float getVocalConfidence() FL_NOEXCEPT;

    // Beat Detection
    float getBeatConfidence() FL_NOEXCEPT;
    float getBPM() FL_NOEXCEPT;

    // Energy Analysis
    float getEnergy() FL_NOEXCEPT;
    float getPeakLevel() FL_NOEXCEPT;

    // Frequency Bands (normalized 0-1, per-band self-referential)
    float getBassLevel() FL_NOEXCEPT;
    float getMidLevel() FL_NOEXCEPT;
    float getTrebleLevel() FL_NOEXCEPT;

    // Frequency Bands (raw unnormalized energy)
    float getBassRaw() FL_NOEXCEPT;
    float getMidRaw() FL_NOEXCEPT;
    float getTrebleRaw() FL_NOEXCEPT;

    // Silence Detection
    bool isSilent() FL_NOEXCEPT;
    u32 getSilenceDuration() FL_NOEXCEPT;

    // Transient Detection
    float getTransientStrength() FL_NOEXCEPT;

    // Dynamics Analysis
    float getDynamicTrend() FL_NOEXCEPT;  // -1.0 to 1.0
    bool isCrescendo() FL_NOEXCEPT;
    bool isDiminuendo() FL_NOEXCEPT;

    // Pitch Detection
    float getPitchConfidence() FL_NOEXCEPT;
    float getPitch() FL_NOEXCEPT;

    // Tempo Analysis
    float getTempoConfidence() FL_NOEXCEPT;
    float getTempoBPM() FL_NOEXCEPT;

    // detector::Buildup Detection
    float getBuildupIntensity() FL_NOEXCEPT;
    float getBuildupProgress() FL_NOEXCEPT;

    // detector::Drop Detection
    float getDropImpact() FL_NOEXCEPT;

    // Percussion Detection
    bool isKick() FL_NOEXCEPT;
    bool isSnare() FL_NOEXCEPT;
    bool isHiHat() FL_NOEXCEPT;
    bool isTom() FL_NOEXCEPT;

    // Note Detection
    u8 getCurrentNote() FL_NOEXCEPT;
    float getNoteVelocity() FL_NOEXCEPT;
    float getNoteConfidence() FL_NOEXCEPT;

    // Downbeat Detection
    float getDownbeatConfidence() FL_NOEXCEPT;
    float getMeasurePhase() FL_NOEXCEPT;
    u8 getCurrentBeatNumber() FL_NOEXCEPT;

    // Backbeat Detection
    float getBackbeatConfidence() FL_NOEXCEPT;
    float getBackbeatStrength() FL_NOEXCEPT;

    // detector::Chord Detection
    float getChordConfidence() FL_NOEXCEPT;

    // detector::Key Detection
    float getKeyConfidence() FL_NOEXCEPT;

    // detector::Mood Analysis
    float getMoodArousal() FL_NOEXCEPT;
    float getMoodValence() FL_NOEXCEPT;  // -1.0 to 1.0

    // Vibe Audio-Reactive (self-normalizing, ~1.0 = average)
    float getVibeBass() FL_NOEXCEPT;      // Immediate relative bass level
    float getVibeMid() FL_NOEXCEPT;       // Immediate relative mid level
    float getVibeTreb() FL_NOEXCEPT;      // Immediate relative treble level
    float getVibeVol() FL_NOEXCEPT;       // Average of bass/mid/treb
    float getVibeBassAtt() FL_NOEXCEPT;   // Smoothed relative bass level
    float getVibeMidAtt() FL_NOEXCEPT;    // Smoothed relative mid level
    float getVibeTrebAtt() FL_NOEXCEPT;   // Smoothed relative treble level
    float getVibeVolAtt() FL_NOEXCEPT;    // Average of smoothed bands
    bool isVibeBassSpike() FL_NOEXCEPT;   // True when bass > bass_att (beat)
    bool isVibeMidSpike() FL_NOEXCEPT;    // True when mid > mid_att
    bool isVibeTrebSpike() FL_NOEXCEPT;   // True when treb > treb_att

    // detector::Equalizer (WLED-style, all 0.0-1.0)
    float getEqBass() FL_NOEXCEPT;
    float getEqMid() FL_NOEXCEPT;
    float getEqTreble() FL_NOEXCEPT;
    float getEqVolume() FL_NOEXCEPT;
    float getEqVolumeNormFactor() FL_NOEXCEPT;
    float getEqZcf() FL_NOEXCEPT;
    float getEqBin(int index) FL_NOEXCEPT;
    float getEqAutoGain() FL_NOEXCEPT;
    bool getEqIsSilence() FL_NOEXCEPT;

    // detector::Equalizer P2: peak detection and dB mapping
    float getEqDominantFreqHz() FL_NOEXCEPT;     ///< Frequency of strongest bin (Hz)
    float getEqDominantMagnitude() FL_NOEXCEPT;  ///< Magnitude of strongest bin (0.0-1.0)
    float getEqVolumeDb() FL_NOEXCEPT;           ///< Volume in approximate dB

    // ----- Configuration -----
    /// Set the sample rate for all frequency-based calculations.
    /// This propagates to Context and all detector.
    /// Default is 44100 Hz. Common values: 16000, 22050, 44100.
    void setSampleRate(int sampleRate) FL_NOEXCEPT;
    int getSampleRate() const FL_NOEXCEPT;

    // ----- Gain Control -----
    /// Set a simple digital gain multiplier applied to each sample before detector.
    /// Default: 1.0 (no change). Values > 1.0 amplify, < 1.0 attenuate.
    void setGain(float gain) FL_NOEXCEPT;
    float getGain() const FL_NOEXCEPT;

    // ----- Signal Conditioning -----
    /// Enable/disable signal conditioning pipeline (DC removal, spike filter, noise gate)
    void setSignalConditioningEnabled(bool enabled) FL_NOEXCEPT;
    /// Enable/disable noise floor tracking
    void setNoiseFloorTrackingEnabled(bool enabled) FL_NOEXCEPT;

    // ----- Microphone Profile -----
    /// Set microphone correction profile for frequency response compensation.
    /// This is a property of the physical microphone hardware.
    /// Propagates to detector::EqualizerDetector as pink noise correction gains.
    void setMicProfile(MicProfile profile) FL_NOEXCEPT;
    MicProfile getMicProfile() const FL_NOEXCEPT { return mMicProfile; }

    /// Configure signal conditioner
    void configureSignalConditioner(const SignalConditionerConfig& config) FL_NOEXCEPT;
    /// Configure noise floor tracker
    void configureNoiseFloorTracker(const NoiseFloorTrackerConfig& config) FL_NOEXCEPT;
    /// Configure equalizer detector tuning parameters
    void configureEqualizer(const detector::EqualizerConfig& config) FL_NOEXCEPT;

    /// Access signal conditioning statistics
    const SignalConditioner::Stats& getSignalConditionerStats() const FL_NOEXCEPT { return mSignalConditioner.getStats(); }
    const NoiseFloorTracker::Stats& getNoiseFloorStats() const FL_NOEXCEPT { return mNoiseFloorTracker.getStats(); }

    // ----- State Access -----
    shared_ptr<Context> getContext() const FL_NOEXCEPT { return mContext; }
    const Sample& getSample() const FL_NOEXCEPT;
    void reset() FL_NOEXCEPT;

private:
    int mSampleRate = 44100;
    float mGain = 1.0f;
    bool mSignalConditioningEnabled = true;
    bool mNoiseFloorTrackingEnabled = false;
    MicProfile mMicProfile = MicProfile::None;
    SignalConditioner mSignalConditioner;
    NoiseFloorTracker mNoiseFloorTracker;
    shared_ptr<Context> mContext;

    // Active detector registry for two-phase update loop
    vector<shared_ptr<Detector>> mActiveDetectors;
    void registerDetector(shared_ptr<Detector> detector) FL_NOEXCEPT;

    // Lazy detector storage
    shared_ptr<detector::Beat> mBeatDetector;
    shared_ptr<detector::FrequencyBands> mFrequencyBands;
    shared_ptr<detector::EnergyAnalyzer> mEnergyAnalyzer;
    shared_ptr<detector::TempoAnalyzer> mTempoAnalyzer;
    shared_ptr<detector::Transient> mTransientDetector;
    shared_ptr<detector::Silence> mSilenceDetector;
    shared_ptr<detector::DynamicsAnalyzer> mDynamicsAnalyzer;
    shared_ptr<detector::Pitch> mPitchDetector;
    shared_ptr<detector::Note> mNoteDetector;
    shared_ptr<detector::Downbeat> mDownbeatDetector;
    shared_ptr<detector::Backbeat> mBackbeatDetector;
    shared_ptr<detector::Vocal> mVocalDetector;
    shared_ptr<detector::Percussion> mPercussionDetector;
    shared_ptr<detector::ChordDetector> mChordDetector;
    shared_ptr<detector::KeyDetector> mKeyDetector;
    shared_ptr<detector::MoodAnalyzer> mMoodAnalyzer;
    shared_ptr<detector::BuildupDetector> mBuildupDetector;
    shared_ptr<detector::DropDetector> mDropDetector;
    shared_ptr<detector::EqualizerDetector> mEqualizerDetector;
    shared_ptr<detector::Vibe> mVibeDetector;

    // Lazy creation helpers
    shared_ptr<detector::Beat> getBeatDetector() FL_NOEXCEPT;
    shared_ptr<detector::FrequencyBands> getFrequencyBands() FL_NOEXCEPT;
    shared_ptr<detector::EnergyAnalyzer> getEnergyAnalyzer() FL_NOEXCEPT;
    shared_ptr<detector::TempoAnalyzer> getTempoAnalyzer() FL_NOEXCEPT;
    shared_ptr<detector::Transient> getTransientDetector() FL_NOEXCEPT;
    shared_ptr<detector::Silence> getSilenceDetector() FL_NOEXCEPT;
    shared_ptr<detector::DynamicsAnalyzer> getDynamicsAnalyzer() FL_NOEXCEPT;
    shared_ptr<detector::Pitch> getPitchDetector() FL_NOEXCEPT;
    shared_ptr<detector::Note> getNoteDetector() FL_NOEXCEPT;
    shared_ptr<detector::Downbeat> getDownbeatDetector() FL_NOEXCEPT;
    shared_ptr<detector::Backbeat> getBackbeatDetector() FL_NOEXCEPT;
    shared_ptr<detector::Vocal> getVocalDetector() FL_NOEXCEPT;
    shared_ptr<detector::Percussion> getPercussionDetector() FL_NOEXCEPT;
    shared_ptr<detector::ChordDetector> getChordDetector() FL_NOEXCEPT;
    shared_ptr<detector::KeyDetector> getKeyDetector() FL_NOEXCEPT;
    shared_ptr<detector::MoodAnalyzer> getMoodAnalyzer() FL_NOEXCEPT;
    shared_ptr<detector::BuildupDetector> getBuildupDetector() FL_NOEXCEPT;
    shared_ptr<detector::DropDetector> getDropDetector() FL_NOEXCEPT;
    shared_ptr<detector::EqualizerDetector> getEqualizerDetector() FL_NOEXCEPT;
    shared_ptr<detector::Vibe> getVibeDetector() FL_NOEXCEPT;

    // Auto-pump support (used by CFastLED::add(Config))
    fl::task::Handle mAutoTask;
    fl::shared_ptr<IInput> mAudioInput;

    static fl::shared_ptr<Processor> createWithAutoInput(
        fl::shared_ptr<IInput> input) FL_NOEXCEPT;
    friend class AudioManager;
};

} // namespace audio
} // namespace fl

#endif // FL_AUDIO_AUDIO_PROCESSOR_H
