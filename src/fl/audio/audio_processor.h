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
    void update(const Sample& sample);

    // ----- Beat Detection Events -----
    void onBeat(function<void()> callback);
    void onBeatPhase(function<void(float phase)> callback);
    void onOnset(function<void(float strength)> callback);
    void onTempoChange(function<void(float bpm, float confidence)> callback);

    // ----- Tempo Analysis Events -----
    void onTempo(function<void(float bpm)> callback);
    void onTempoWithConfidence(function<void(float bpm, float confidence)> callback);
    void onTempoStable(function<void()> callback);
    void onTempoUnstable(function<void()> callback);

    // ----- Frequency Band Events -----
    void onBass(function<void(float level)> callback);
    void onMid(function<void(float level)> callback);
    void onTreble(function<void(float level)> callback);
    void onFrequencyBands(function<void(float bass, float mid, float treble)> callback);

    // ----- detector::Equalizer Events (WLED-style, all 0.0-1.0) -----
    void onEqualizer(function<void(const detector::Equalizer&)> callback);

    // ----- Energy/Level Events -----
    void onEnergy(function<void(float rms)> callback);
    void onNormalizedEnergy(function<void(float normalizedRms)> callback);
    void onPeak(function<void(float peak)> callback);
    void onAverageEnergy(function<void(float avgEnergy)> callback);

    // ----- Transient Detection Events -----
    void onTransient(function<void()> callback);
    void onTransientWithStrength(function<void(float strength)> callback);
    void onAttack(function<void(float strength)> callback);

    // ----- Silence Detection Events -----
    void onSilence(function<void(u8 silent)> callback);
    void onSilenceStart(function<void()> callback);
    void onSilenceEnd(function<void()> callback);
    void onSilenceDuration(function<void(u32 durationMs)> callback);

    // ----- Dynamics Analysis Events -----
    void onCrescendo(function<void()> callback);
    void onDiminuendo(function<void()> callback);
    void onDynamicTrend(function<void(float trend)> callback);
    void onCompressionRatio(function<void(float compression)> callback);

    // ----- Pitch Detection Events -----
    void onPitch(function<void(float hz)> callback);
    void onPitchWithConfidence(function<void(float hz, float confidence)> callback);
    void onPitchChange(function<void(float hz)> callback);
    void onVoiced(function<void(u8 voiced)> callback);

    // ----- Note Detection Events -----
    void onNoteOn(function<void(u8 note, u8 velocity)> callback);
    void onNoteOff(function<void(u8 note)> callback);
    void onNoteChange(function<void(u8 note, u8 velocity)> callback);

    // ----- Downbeat Detection Events -----
    void onDownbeat(function<void()> callback);
    void onMeasureBeat(function<void(u8 beatNumber)> callback);
    void onMeterChange(function<void(u8 beatsPerMeasure)> callback);
    void onMeasurePhase(function<void(float phase)> callback);

    // ----- Backbeat Detection Events -----
    void onBackbeat(function<void(u8 beatNumber, float confidence, float strength)> callback);

    // ----- Vocal Detection Events -----
    void onVocal(function<void(u8 active)> callback);
    void onVocalStart(function<void()> callback);
    void onVocalEnd(function<void()> callback);
    void onVocalConfidence(function<void(float confidence)> callback);

    // ----- Percussion Detection Events -----
    void onPercussion(function<void(detector::PercussionType type)> callback);
    void onKick(function<void()> callback);
    void onSnare(function<void()> callback);
    void onHiHat(function<void()> callback);
    void onTom(function<void()> callback);

    // ----- detector::Chord Detection Events -----
    void onChord(function<void(const detector::Chord& chord)> callback);
    void onChordChange(function<void(const detector::Chord& chord)> callback);
    void onChordEnd(function<void()> callback);

    // ----- detector::Key Detection Events -----
    void onKey(function<void(const detector::Key& key)> callback);
    void onKeyChange(function<void(const detector::Key& key)> callback);
    void onKeyEnd(function<void()> callback);

    // ----- detector::Mood Analysis Events -----
    void onMood(function<void(const detector::Mood& mood)> callback);
    void onMoodChange(function<void(const detector::Mood& mood)> callback);
    void onValenceArousal(function<void(float valence, float arousal)> callback);

    // ----- detector::Buildup Detection Events -----
    void onBuildupStart(function<void()> callback);
    void onBuildupProgress(function<void(float progress)> callback);
    void onBuildupPeak(function<void()> callback);
    void onBuildupEnd(function<void()> callback);
    void onBuildup(function<void(const detector::Buildup&)> callback);

    // ----- detector::Drop Detection Events -----
    void onDrop(function<void()> callback);
    void onDropEvent(function<void(const detector::Drop&)> callback);
    void onDropImpact(function<void(float impact)> callback);

    // ----- Vibe Audio-Reactive Events -----
    // Comprehensive self-normalizing levels, spikes, and raw values
    void onVibeLevels(function<void(const detector::VibeLevels&)> callback);
    void onVibeBassSpike(function<void()> callback);
    void onVibeMidSpike(function<void()> callback);
    void onVibeTrebSpike(function<void()> callback);

    // ----- Polling Getters (float 0.0-1.0, bool, or integer) -----

    // Vocal Detection
    float getVocalConfidence();

    // Beat Detection
    float getBeatConfidence();
    float getBPM();

    // Energy Analysis
    float getEnergy();
    float getPeakLevel();

    // Frequency Bands (normalized 0-1, per-band self-referential)
    float getBassLevel();
    float getMidLevel();
    float getTrebleLevel();

    // Frequency Bands (raw unnormalized energy)
    float getBassRaw();
    float getMidRaw();
    float getTrebleRaw();

    // Silence Detection
    bool isSilent();
    u32 getSilenceDuration();

    // Transient Detection
    float getTransientStrength();

    // Dynamics Analysis
    float getDynamicTrend();  // -1.0 to 1.0
    bool isCrescendo();
    bool isDiminuendo();

    // Pitch Detection
    float getPitchConfidence();
    float getPitch();

    // Tempo Analysis
    float getTempoConfidence();
    float getTempoBPM();

    // detector::Buildup Detection
    float getBuildupIntensity();
    float getBuildupProgress();

    // detector::Drop Detection
    float getDropImpact();

    // Percussion Detection
    bool isKick();
    bool isSnare();
    bool isHiHat();
    bool isTom();

    // Note Detection
    u8 getCurrentNote();
    float getNoteVelocity();
    float getNoteConfidence();

    // Downbeat Detection
    float getDownbeatConfidence();
    float getMeasurePhase();
    u8 getCurrentBeatNumber();

    // Backbeat Detection
    float getBackbeatConfidence();
    float getBackbeatStrength();

    // detector::Chord Detection
    float getChordConfidence();

    // detector::Key Detection
    float getKeyConfidence();

    // detector::Mood Analysis
    float getMoodArousal();
    float getMoodValence();  // -1.0 to 1.0

    // Vibe Audio-Reactive (self-normalizing, ~1.0 = average)
    float getVibeBass();      // Immediate relative bass level
    float getVibeMid();       // Immediate relative mid level
    float getVibeTreb();      // Immediate relative treble level
    float getVibeVol();       // Average of bass/mid/treb
    float getVibeBassAtt();   // Smoothed relative bass level
    float getVibeMidAtt();    // Smoothed relative mid level
    float getVibeTrebAtt();   // Smoothed relative treble level
    float getVibeVolAtt();    // Average of smoothed bands
    bool isVibeBassSpike();   // True when bass > bass_att (beat)
    bool isVibeMidSpike();    // True when mid > mid_att
    bool isVibeTrebSpike();   // True when treb > treb_att

    // detector::Equalizer (WLED-style, all 0.0-1.0)
    float getEqBass();
    float getEqMid();
    float getEqTreble();
    float getEqVolume();
    float getEqVolumeNormFactor();
    float getEqZcf();
    float getEqBin(int index);
    float getEqAutoGain();
    bool getEqIsSilence();

    // detector::Equalizer P2: peak detection and dB mapping
    float getEqDominantFreqHz();     ///< Frequency of strongest bin (Hz)
    float getEqDominantMagnitude();  ///< Magnitude of strongest bin (0.0-1.0)
    float getEqVolumeDb();           ///< Volume in approximate dB

    // ----- Configuration -----
    /// Set the sample rate for all frequency-based calculations.
    /// This propagates to Context and all detector.
    /// Default is 44100 Hz. Common values: 16000, 22050, 44100.
    void setSampleRate(int sampleRate);
    int getSampleRate() const;

    // ----- Gain Control -----
    /// Set a simple digital gain multiplier applied to each sample before detector.
    /// Default: 1.0 (no change). Values > 1.0 amplify, < 1.0 attenuate.
    void setGain(float gain);
    float getGain() const;

    // ----- Signal Conditioning -----
    /// Enable/disable signal conditioning pipeline (DC removal, spike filter, noise gate)
    void setSignalConditioningEnabled(bool enabled);
    /// Enable/disable noise floor tracking
    void setNoiseFloorTrackingEnabled(bool enabled);

    // ----- Microphone Profile -----
    /// Set microphone correction profile for frequency response compensation.
    /// This is a property of the physical microphone hardware.
    /// Propagates to detector::EqualizerDetector as pink noise correction gains.
    void setMicProfile(MicProfile profile);
    MicProfile getMicProfile() const { return mMicProfile; }

    /// Configure signal conditioner
    void configureSignalConditioner(const SignalConditionerConfig& config);
    /// Configure noise floor tracker
    void configureNoiseFloorTracker(const NoiseFloorTrackerConfig& config);
    /// Configure equalizer detector tuning parameters
    void configureEqualizer(const detector::EqualizerConfig& config);

    /// Access signal conditioning statistics
    const SignalConditioner::Stats& getSignalConditionerStats() const { return mSignalConditioner.getStats(); }
    const NoiseFloorTracker::Stats& getNoiseFloorStats() const { return mNoiseFloorTracker.getStats(); }

    // ----- State Access -----
    shared_ptr<Context> getContext() const { return mContext; }
    const Sample& getSample() const;
    void reset();

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
    void registerDetector(shared_ptr<Detector> detector);

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
    shared_ptr<detector::Beat> getBeatDetector();
    shared_ptr<detector::FrequencyBands> getFrequencyBands();
    shared_ptr<detector::EnergyAnalyzer> getEnergyAnalyzer();
    shared_ptr<detector::TempoAnalyzer> getTempoAnalyzer();
    shared_ptr<detector::Transient> getTransientDetector();
    shared_ptr<detector::Silence> getSilenceDetector();
    shared_ptr<detector::DynamicsAnalyzer> getDynamicsAnalyzer();
    shared_ptr<detector::Pitch> getPitchDetector();
    shared_ptr<detector::Note> getNoteDetector();
    shared_ptr<detector::Downbeat> getDownbeatDetector();
    shared_ptr<detector::Backbeat> getBackbeatDetector();
    shared_ptr<detector::Vocal> getVocalDetector();
    shared_ptr<detector::Percussion> getPercussionDetector();
    shared_ptr<detector::ChordDetector> getChordDetector();
    shared_ptr<detector::KeyDetector> getKeyDetector();
    shared_ptr<detector::MoodAnalyzer> getMoodAnalyzer();
    shared_ptr<detector::BuildupDetector> getBuildupDetector();
    shared_ptr<detector::DropDetector> getDropDetector();
    shared_ptr<detector::EqualizerDetector> getEqualizerDetector();
    shared_ptr<detector::Vibe> getVibeDetector();

    // Auto-pump support (used by CFastLED::add(Config))
    fl::task::Handle mAutoTask;
    fl::shared_ptr<IInput> mAudioInput;

    static fl::shared_ptr<Processor> createWithAutoInput(
        fl::shared_ptr<IInput> input);
    friend class AudioManager;
};

} // namespace audio
} // namespace fl

#endif // FL_AUDIO_AUDIO_PROCESSOR_H
