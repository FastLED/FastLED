#pragma once

#include "fl/audio.h"
#include "fl/audio/audio_context.h"
#include "fl/audio/audio_detector.h"
#include "fl/audio/signal_conditioner.h"
#include "fl/audio/auto_gain.h"
#include "fl/audio/noise_floor_tracker.h"
#include "fl/stl/shared_ptr.h"
#include "fl/stl/function.h"
#include "fl/stl/vector.h"

namespace fl {

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

    // ----- Polling Getters (uint8_t-scaled where applicable) -----

    // Vocal Detection
    u8 getVocalConfidence();
    u8 isVocalActive();

    // Beat Detection
    u8 getBeatConfidence();
    u8 isBeat();
    float getBPM();

    // Energy Analysis
    u8 getEnergy();
    u8 getPeakLevel();

    // Frequency Bands
    u8 getBassLevel();
    u8 getMidLevel();
    u8 getTrebleLevel();

    // Silence Detection
    u8 isSilent();
    u32 getSilenceDuration();

    // Transient Detection
    u8 getTransientStrength();
    u8 isTransient();

    // Dynamics Analysis
    u8 getDynamicTrend();
    u8 isCrescendo();
    u8 isDiminuendo();

    // Pitch Detection
    u8 getPitchConfidence();
    float getPitch();
    u8 isVoiced();

    // Tempo Analysis
    u8 getTempoConfidence();
    float getTempoBPM();
    u8 isTempoStable();

    // Buildup Detection
    u8 getBuildupIntensity();
    u8 getBuildupProgress();
    u8 isBuilding();

    // Drop Detection
    u8 getDropImpact();

    // Percussion Detection
    u8 isKick();
    u8 isSnare();
    u8 isHiHat();
    u8 isTom();

    // Note Detection
    u8 getCurrentNote();
    u8 getNoteVelocity();
    u8 isNoteActive();

    // Downbeat Detection
    u8 isDownbeat();
    u8 getMeasurePhase();
    u8 getCurrentBeatNumber();

    // Backbeat Detection
    u8 getBackbeatConfidence();
    u8 getBackbeatStrength();

    // Chord Detection
    u8 hasChord();
    u8 getChordConfidence();

    // Key Detection
    u8 hasKey();
    u8 getKeyConfidence();

    // Mood Analysis
    u8 getMoodArousal();
    u8 getMoodValence();

    // ----- Configuration -----
    /// Set the sample rate for all frequency-based calculations.
    /// This propagates to AudioContext and all detectors.
    /// Default is 44100 Hz. Common values: 16000, 22050, 44100.
    void setSampleRate(int sampleRate);
    int getSampleRate() const;

    // ----- Signal Conditioning -----
    /// Enable/disable signal conditioning pipeline (DC removal, spike filter, noise gate)
    void setSignalConditioningEnabled(bool enabled);
    /// Enable/disable automatic gain control
    void setAutoGainEnabled(bool enabled);
    /// Enable/disable noise floor tracking
    void setNoiseFloorTrackingEnabled(bool enabled);

    /// Configure signal conditioner
    void configureSignalConditioner(const SignalConditionerConfig& config);
    /// Configure auto gain
    void configureAutoGain(const AutoGainConfig& config);
    /// Configure noise floor tracker
    void configureNoiseFloorTracker(const NoiseFloorTrackerConfig& config);

    /// Access signal conditioning statistics
    const SignalConditioner::Stats& getSignalConditionerStats() const { return mSignalConditioner.getStats(); }
    const AutoGain::Stats& getAutoGainStats() const { return mAutoGain.getStats(); }
    const NoiseFloorTracker::Stats& getNoiseFloorStats() const { return mNoiseFloorTracker.getStats(); }

    // ----- State Access -----
    shared_ptr<AudioContext> getContext() const { return mContext; }
    const AudioSample& getSample() const;
    void reset();

private:
    int mSampleRate = 44100;
    bool mSignalConditioningEnabled = true;
    bool mAutoGainEnabled = false;
    bool mNoiseFloorTrackingEnabled = false;
    SignalConditioner mSignalConditioner;
    AutoGain mAutoGain;
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
};

} // namespace fl
