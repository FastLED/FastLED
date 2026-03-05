#pragma once

#ifndef FL_AUDIO_AUDIO_PROCESSOR_H
#define FL_AUDIO_AUDIO_PROCESSOR_H

#include "fl/audio.h"  // IWYU pragma: keep
#include "fl/audio/audio_context.h"  // IWYU pragma: keep
#include "fl/audio/audio_detector.h"  // IWYU pragma: keep
#include "fl/audio/detectors/vibe.h"  // IWYU pragma: keep - VibeLevels used in public callback API
#include "fl/audio/mic_profiles.h"
#include "fl/audio/signal_conditioner.h"
#include "fl/audio/noise_floor_tracker.h"
#include "fl/stl/shared_ptr.h"
#include "fl/stl/function.h"  // IWYU pragma: keep
#include "fl/stl/vector.h"
#include "fl/task.h"

class CFastLED;

namespace fl {

class IAudioInput;

class BeatDetector;
class FrequencyBands;
class EnergyAnalyzer;
class TempoAnalyzer;
class TransientDetector;
class SilenceDetector;
class DynamicsAnalyzer;
class PitchDetector;
class NoteDetector;
class DownbeatDetector;
class BackbeatDetector;
class VocalDetector;
class PercussionDetector;
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
class VibeDetector;

class AudioProcessor {
public:
    AudioProcessor();
    ~AudioProcessor();

    // ----- Main Update -----
    void update(const AudioSample& sample);

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

    // ----- Equalizer Events (WLED-style, all 0.0-1.0) -----
    void onEqualizer(function<void(const Equalizer&)> callback);

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
    void onPercussion(function<void(PercussionType type)> callback);
    void onKick(function<void()> callback);
    void onSnare(function<void()> callback);
    void onHiHat(function<void()> callback);
    void onTom(function<void()> callback);

    // ----- Chord Detection Events -----
    void onChord(function<void(const Chord& chord)> callback);
    void onChordChange(function<void(const Chord& chord)> callback);
    void onChordEnd(function<void()> callback);

    // ----- Key Detection Events -----
    void onKey(function<void(const Key& key)> callback);
    void onKeyChange(function<void(const Key& key)> callback);
    void onKeyEnd(function<void()> callback);

    // ----- Mood Analysis Events -----
    void onMood(function<void(const Mood& mood)> callback);
    void onMoodChange(function<void(const Mood& mood)> callback);
    void onValenceArousal(function<void(float valence, float arousal)> callback);

    // ----- Buildup Detection Events -----
    void onBuildupStart(function<void()> callback);
    void onBuildupProgress(function<void(float progress)> callback);
    void onBuildupPeak(function<void()> callback);
    void onBuildupEnd(function<void()> callback);
    void onBuildup(function<void(const Buildup&)> callback);

    // ----- Drop Detection Events -----
    void onDrop(function<void()> callback);
    void onDropEvent(function<void(const Drop&)> callback);
    void onDropImpact(function<void(float impact)> callback);

    // ----- Vibe Audio-Reactive Events -----
    // Comprehensive self-normalizing levels, spikes, and raw values
    void onVibeLevels(function<void(const VibeLevels&)> callback);
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

    // Buildup Detection
    float getBuildupIntensity();
    float getBuildupProgress();

    // Drop Detection
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

    // Chord Detection
    float getChordConfidence();

    // Key Detection
    float getKeyConfidence();

    // Mood Analysis
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

    // Equalizer (WLED-style, all 0.0-1.0)
    float getEqBass();
    float getEqMid();
    float getEqTreble();
    float getEqVolume();
    float getEqZcf();
    float getEqBin(int index);
    float getEqAutoGain();
    bool getEqIsSilence();

    // Equalizer P2: peak detection and dB mapping
    float getEqDominantFreqHz();     ///< Frequency of strongest bin (Hz)
    float getEqDominantMagnitude();  ///< Magnitude of strongest bin (0.0-1.0)
    float getEqVolumeDb();           ///< Volume in approximate dB

    // ----- Configuration -----
    /// Set the sample rate for all frequency-based calculations.
    /// This propagates to AudioContext and all detectors.
    /// Default is 44100 Hz. Common values: 16000, 22050, 44100.
    void setSampleRate(int sampleRate);
    int getSampleRate() const;

    // ----- Gain Control -----
    /// Set a simple digital gain multiplier applied to each sample before detectors.
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
    /// Propagates to EqualizerDetector as pink noise correction gains.
    void setMicProfile(MicProfile profile);
    MicProfile getMicProfile() const { return mMicProfile; }

    /// Configure signal conditioner
    void configureSignalConditioner(const SignalConditionerConfig& config);
    /// Configure noise floor tracker
    void configureNoiseFloorTracker(const NoiseFloorTrackerConfig& config);
    /// Configure equalizer detector tuning parameters
    void configureEqualizer(const EqualizerConfig& config);

    /// Access signal conditioning statistics
    const SignalConditioner::Stats& getSignalConditionerStats() const { return mSignalConditioner.getStats(); }
    const NoiseFloorTracker::Stats& getNoiseFloorStats() const { return mNoiseFloorTracker.getStats(); }

    // ----- State Access -----
    shared_ptr<AudioContext> getContext() const { return mContext; }
    const AudioSample& getSample() const;
    void reset();

private:
    int mSampleRate = 44100;
    float mGain = 1.0f;
    bool mSignalConditioningEnabled = true;
    bool mNoiseFloorTrackingEnabled = false;
    MicProfile mMicProfile = MicProfile::None;
    SignalConditioner mSignalConditioner;
    NoiseFloorTracker mNoiseFloorTracker;
    shared_ptr<AudioContext> mContext;

    // Active detector registry for two-phase update loop
    vector<shared_ptr<AudioDetector>> mActiveDetectors;
    void registerDetector(shared_ptr<AudioDetector> detector);

    // Lazy detector storage
    shared_ptr<BeatDetector> mBeatDetector;
    shared_ptr<FrequencyBands> mFrequencyBands;
    shared_ptr<EnergyAnalyzer> mEnergyAnalyzer;
    shared_ptr<TempoAnalyzer> mTempoAnalyzer;
    shared_ptr<TransientDetector> mTransientDetector;
    shared_ptr<SilenceDetector> mSilenceDetector;
    shared_ptr<DynamicsAnalyzer> mDynamicsAnalyzer;
    shared_ptr<PitchDetector> mPitchDetector;
    shared_ptr<NoteDetector> mNoteDetector;
    shared_ptr<DownbeatDetector> mDownbeatDetector;
    shared_ptr<BackbeatDetector> mBackbeatDetector;
    shared_ptr<VocalDetector> mVocalDetector;
    shared_ptr<PercussionDetector> mPercussionDetector;
    shared_ptr<ChordDetector> mChordDetector;
    shared_ptr<KeyDetector> mKeyDetector;
    shared_ptr<MoodAnalyzer> mMoodAnalyzer;
    shared_ptr<BuildupDetector> mBuildupDetector;
    shared_ptr<DropDetector> mDropDetector;
    shared_ptr<EqualizerDetector> mEqualizerDetector;
    shared_ptr<VibeDetector> mVibeDetector;

    // Lazy creation helpers
    shared_ptr<BeatDetector> getBeatDetector();
    shared_ptr<FrequencyBands> getFrequencyBands();
    shared_ptr<EnergyAnalyzer> getEnergyAnalyzer();
    shared_ptr<TempoAnalyzer> getTempoAnalyzer();
    shared_ptr<TransientDetector> getTransientDetector();
    shared_ptr<SilenceDetector> getSilenceDetector();
    shared_ptr<DynamicsAnalyzer> getDynamicsAnalyzer();
    shared_ptr<PitchDetector> getPitchDetector();
    shared_ptr<NoteDetector> getNoteDetector();
    shared_ptr<DownbeatDetector> getDownbeatDetector();
    shared_ptr<BackbeatDetector> getBackbeatDetector();
    shared_ptr<VocalDetector> getVocalDetector();
    shared_ptr<PercussionDetector> getPercussionDetector();
    shared_ptr<ChordDetector> getChordDetector();
    shared_ptr<KeyDetector> getKeyDetector();
    shared_ptr<MoodAnalyzer> getMoodAnalyzer();
    shared_ptr<BuildupDetector> getBuildupDetector();
    shared_ptr<DropDetector> getDropDetector();
    shared_ptr<EqualizerDetector> getEqualizerDetector();
    shared_ptr<VibeDetector> getVibeDetector();

    // Auto-pump support (used by CFastLED::add(AudioConfig))
    fl::task mAutoTask;
    fl::shared_ptr<IAudioInput> mAudioInput;

    static fl::shared_ptr<AudioProcessor> createWithAutoInput(
        fl::shared_ptr<IAudioInput> input);
    friend class ::CFastLED;
};

} // namespace fl

#endif // FL_AUDIO_AUDIO_PROCESSOR_H
