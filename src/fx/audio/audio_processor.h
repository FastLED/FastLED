#pragma once

#include "fl/audio.h"
#include "fl/audio/audio_context.h"
#include "fl/audio/audio_detector.h"
#include "fl/stl/shared_ptr.h"
#include "fl/stl/function.h"

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
    void onPeak(function<void(float peak)> callback);
    void onAverageEnergy(function<void(float avgEnergy)> callback);

    // ----- Transient Detection Events -----
    void onTransient(function<void()> callback);
    void onTransientWithStrength(function<void(float strength)> callback);
    void onAttack(function<void(float strength)> callback);

    // ----- Silence Detection Events -----
    void onSilence(function<void(bool silent)> callback);
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
    void onVoicedChange(function<void(bool voiced)> callback);

    // ----- Note Detection Events -----
    void onNoteOn(function<void(uint8_t note, uint8_t velocity)> callback);
    void onNoteOff(function<void(uint8_t note)> callback);
    void onNoteChange(function<void(uint8_t note, uint8_t velocity)> callback);

    // ----- Downbeat Detection Events -----
    void onDownbeat(function<void()> callback);
    void onMeasureBeat(function<void(u8 beatNumber)> callback);
    void onMeterChange(function<void(u8 beatsPerMeasure)> callback);
    void onMeasurePhase(function<void(float phase)> callback);

    // ----- Backbeat Detection Events -----
    void onBackbeat(function<void(u8 beatNumber, float confidence, float strength)> callback);

    // ----- Vocal Detection Events -----
    void onVocal(function<void(bool active)> callback);
    void onVocalStart(function<void()> callback);
    void onVocalEnd(function<void()> callback);
    void onVocalConfidence(function<void(float confidence)> callback);

    // ----- Percussion Detection Events -----
    void onPercussion(function<void(const char* type)> callback);
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

    // ----- State Access -----
    shared_ptr<AudioContext> getContext() const { return mContext; }
    const AudioSample& getSample() const;
    void reset();

private:
    shared_ptr<AudioContext> mContext;

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
