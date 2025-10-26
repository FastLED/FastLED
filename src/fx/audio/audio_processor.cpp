#include "fx/audio/audio_processor.h"
#include "fx/audio/detectors/beat.h"
#include "fx/audio/detectors/frequency_bands.h"
#include "fx/audio/detectors/energy_analyzer.h"
#include "fx/audio/detectors/tempo_analyzer.h"
#include "fx/audio/detectors/transient.h"
#include "fx/audio/detectors/silence.h"
#include "fx/audio/detectors/dynamics_analyzer.h"
#include "fx/audio/detectors/pitch.h"
#include "fx/audio/detectors/note.h"
#include "fx/audio/detectors/downbeat.h"
#include "fx/audio/detectors/backbeat.h"
#include "fx/audio/detectors/vocal.h"
#include "fx/audio/detectors/percussion.h"
#include "fx/audio/detectors/chord.h"
#include "fx/audio/detectors/key.h"
#include "fx/audio/detectors/mood_analyzer.h"
#include "fx/audio/detectors/buildup.h"
#include "fx/audio/detectors/drop.h"

namespace fl {

AudioProcessor::AudioProcessor()
    : mContext(make_shared<AudioContext>(AudioSample()))
{}

AudioProcessor::~AudioProcessor() = default;

void AudioProcessor::update(const AudioSample& sample) {
    mContext->setSample(sample);

    if (mBeatDetector) {
        mBeatDetector->update(mContext);
    }
    if (mFrequencyBands) {
        mFrequencyBands->update(mContext);
    }
    if (mEnergyAnalyzer) {
        mEnergyAnalyzer->update(mContext);
    }
    if (mTempoAnalyzer) {
        mTempoAnalyzer->update(mContext);
    }
    if (mTransientDetector) {
        mTransientDetector->update(mContext);
    }
    if (mSilenceDetector) {
        mSilenceDetector->update(mContext);
    }
    if (mDynamicsAnalyzer) {
        mDynamicsAnalyzer->update(mContext);
    }
    if (mPitchDetector) {
        mPitchDetector->update(mContext);
    }
    if (mNoteDetector) {
        mNoteDetector->update(mContext);
    }
    if (mDownbeatDetector) {
        mDownbeatDetector->update(mContext);
    }
    if (mBackbeatDetector) {
        mBackbeatDetector->update(mContext);
    }
    if (mVocalDetector) {
        mVocalDetector->update(mContext);
    }
    if (mPercussionDetector) {
        mPercussionDetector->update(mContext);
    }
    if (mChordDetector) {
        mChordDetector->update(mContext);
    }
    if (mKeyDetector) {
        mKeyDetector->update(mContext);
    }
    if (mMoodAnalyzer) {
        mMoodAnalyzer->update(mContext);
    }
    if (mBuildupDetector) {
        mBuildupDetector->update(mContext);
    }
    if (mDropDetector) {
        mDropDetector->update(mContext);
    }
}

void AudioProcessor::onBeat(function<void()> callback) {
    auto detector = getBeatDetector();
    detector->onBeat = callback;
}

void AudioProcessor::onBeatPhase(function<void(float)> callback) {
    auto detector = getBeatDetector();
    detector->onBeatPhase = callback;
}

void AudioProcessor::onOnset(function<void(float)> callback) {
    auto detector = getBeatDetector();
    detector->onOnset = callback;
}

void AudioProcessor::onTempoChange(function<void(float, float)> callback) {
    auto detector = getBeatDetector();
    detector->onTempoChange = callback;
}

void AudioProcessor::onTempo(function<void(float)> callback) {
    auto detector = getTempoAnalyzer();
    detector->onTempo = callback;
}

void AudioProcessor::onTempoWithConfidence(function<void(float, float)> callback) {
    auto detector = getTempoAnalyzer();
    detector->onTempoWithConfidence = callback;
}

void AudioProcessor::onTempoStable(function<void()> callback) {
    auto detector = getTempoAnalyzer();
    detector->onTempoStable = callback;
}

void AudioProcessor::onTempoUnstable(function<void()> callback) {
    auto detector = getTempoAnalyzer();
    detector->onTempoUnstable = callback;
}

void AudioProcessor::onBass(function<void(float)> callback) {
    auto detector = getFrequencyBands();
    detector->onBassLevel = callback;
}

void AudioProcessor::onMid(function<void(float)> callback) {
    auto detector = getFrequencyBands();
    detector->onMidLevel = callback;
}

void AudioProcessor::onTreble(function<void(float)> callback) {
    auto detector = getFrequencyBands();
    detector->onTrebleLevel = callback;
}

void AudioProcessor::onFrequencyBands(function<void(float, float, float)> callback) {
    auto detector = getFrequencyBands();
    detector->onLevelsUpdate = callback;
}

void AudioProcessor::onEnergy(function<void(float)> callback) {
    auto detector = getEnergyAnalyzer();
    detector->onEnergy = callback;
}

void AudioProcessor::onPeak(function<void(float)> callback) {
    auto detector = getEnergyAnalyzer();
    detector->onPeak = callback;
}

void AudioProcessor::onAverageEnergy(function<void(float)> callback) {
    auto detector = getEnergyAnalyzer();
    detector->onAverageEnergy = callback;
}

void AudioProcessor::onTransient(function<void()> callback) {
    auto detector = getTransientDetector();
    detector->onTransient = callback;
}

void AudioProcessor::onTransientWithStrength(function<void(float)> callback) {
    auto detector = getTransientDetector();
    detector->onTransientWithStrength = callback;
}

void AudioProcessor::onAttack(function<void(float)> callback) {
    auto detector = getTransientDetector();
    detector->onAttack = callback;
}

void AudioProcessor::onSilence(function<void(bool)> callback) {
    auto detector = getSilenceDetector();
    detector->onSilenceChange = callback;
}

void AudioProcessor::onSilenceStart(function<void()> callback) {
    auto detector = getSilenceDetector();
    detector->onSilenceStart = callback;
}

void AudioProcessor::onSilenceEnd(function<void()> callback) {
    auto detector = getSilenceDetector();
    detector->onSilenceEnd = callback;
}

void AudioProcessor::onSilenceDuration(function<void(u32)> callback) {
    auto detector = getSilenceDetector();
    detector->onSilenceDuration = callback;
}

void AudioProcessor::onCrescendo(function<void()> callback) {
    auto detector = getDynamicsAnalyzer();
    detector->onCrescendo = callback;
}

void AudioProcessor::onDiminuendo(function<void()> callback) {
    auto detector = getDynamicsAnalyzer();
    detector->onDiminuendo = callback;
}

void AudioProcessor::onDynamicTrend(function<void(float)> callback) {
    auto detector = getDynamicsAnalyzer();
    detector->onDynamicTrend = callback;
}

void AudioProcessor::onCompressionRatio(function<void(float)> callback) {
    auto detector = getDynamicsAnalyzer();
    detector->onCompressionRatio = callback;
}

void AudioProcessor::onPitch(function<void(float)> callback) {
    auto detector = getPitchDetector();
    detector->onPitch = callback;
}

void AudioProcessor::onPitchWithConfidence(function<void(float, float)> callback) {
    auto detector = getPitchDetector();
    detector->onPitchWithConfidence = callback;
}

void AudioProcessor::onPitchChange(function<void(float)> callback) {
    auto detector = getPitchDetector();
    detector->onPitchChange = callback;
}

void AudioProcessor::onVoicedChange(function<void(bool)> callback) {
    auto detector = getPitchDetector();
    detector->onVoicedChange = callback;
}

void AudioProcessor::onNoteOn(function<void(uint8_t, uint8_t)> callback) {
    auto detector = getNoteDetector();
    detector->onNoteOn = callback;
}

void AudioProcessor::onNoteOff(function<void(uint8_t)> callback) {
    auto detector = getNoteDetector();
    detector->onNoteOff = callback;
}

void AudioProcessor::onNoteChange(function<void(uint8_t, uint8_t)> callback) {
    auto detector = getNoteDetector();
    detector->onNoteChange = callback;
}

void AudioProcessor::onDownbeat(function<void()> callback) {
    auto detector = getDownbeatDetector();
    detector->onDownbeat = callback;
}

void AudioProcessor::onMeasureBeat(function<void(u8)> callback) {
    auto detector = getDownbeatDetector();
    detector->onMeasureBeat = callback;
}

void AudioProcessor::onMeterChange(function<void(u8)> callback) {
    auto detector = getDownbeatDetector();
    detector->onMeterChange = callback;
}

void AudioProcessor::onMeasurePhase(function<void(float)> callback) {
    auto detector = getDownbeatDetector();
    detector->onMeasurePhase = callback;
}

void AudioProcessor::onBackbeat(function<void(u8 beatNumber, float confidence, float strength)> callback) {
    auto detector = getBackbeatDetector();
    detector->onBackbeat = callback;
}

void AudioProcessor::onVocal(function<void(bool)> callback) {
    auto detector = getVocalDetector();
    detector->onVocalChange = callback;
}

void AudioProcessor::onVocalStart(function<void()> callback) {
    auto detector = getVocalDetector();
    detector->onVocalStart = callback;
}

void AudioProcessor::onVocalEnd(function<void()> callback) {
    auto detector = getVocalDetector();
    detector->onVocalEnd = callback;
}

void AudioProcessor::onVocalConfidence(function<void(float)> callback) {
    auto detector = getVocalDetector();
    // This callback fires every frame with the current confidence
    // We need to wrap it since VocalDetector doesn't have this callback built-in
    detector->onVocalChange = [callback, detector](bool) {
        if (callback) {
            callback(detector->getConfidence());
        }
    };
}

void AudioProcessor::onPercussion(function<void(const char*)> callback) {
    auto detector = getPercussionDetector();
    detector->onPercussionHit = callback;
}

void AudioProcessor::onKick(function<void()> callback) {
    auto detector = getPercussionDetector();
    detector->onKick = callback;
}

void AudioProcessor::onSnare(function<void()> callback) {
    auto detector = getPercussionDetector();
    detector->onSnare = callback;
}

void AudioProcessor::onHiHat(function<void()> callback) {
    auto detector = getPercussionDetector();
    detector->onHiHat = callback;
}

void AudioProcessor::onTom(function<void()> callback) {
    auto detector = getPercussionDetector();
    detector->onTom = callback;
}

void AudioProcessor::onChord(function<void(const Chord&)> callback) {
    auto detector = getChordDetector();
    detector->onChord = callback;
}

void AudioProcessor::onChordChange(function<void(const Chord&)> callback) {
    auto detector = getChordDetector();
    detector->onChordChange = callback;
}

void AudioProcessor::onChordEnd(function<void()> callback) {
    auto detector = getChordDetector();
    detector->onChordEnd = callback;
}

void AudioProcessor::onKey(function<void(const Key&)> callback) {
    auto detector = getKeyDetector();
    detector->onKey = callback;
}

void AudioProcessor::onKeyChange(function<void(const Key&)> callback) {
    auto detector = getKeyDetector();
    detector->onKeyChange = callback;
}

void AudioProcessor::onKeyEnd(function<void()> callback) {
    auto detector = getKeyDetector();
    detector->onKeyEnd = callback;
}

void AudioProcessor::onMood(function<void(const Mood&)> callback) {
    auto detector = getMoodAnalyzer();
    detector->onMood = callback;
}

void AudioProcessor::onMoodChange(function<void(const Mood&)> callback) {
    auto detector = getMoodAnalyzer();
    detector->onMoodChange = callback;
}

void AudioProcessor::onValenceArousal(function<void(float, float)> callback) {
    auto detector = getMoodAnalyzer();
    detector->onValenceArousal = callback;
}

void AudioProcessor::onBuildupStart(function<void()> callback) {
    auto detector = getBuildupDetector();
    detector->onBuildupStart = callback;
}

void AudioProcessor::onBuildupProgress(function<void(float)> callback) {
    auto detector = getBuildupDetector();
    detector->onBuildupProgress = callback;
}

void AudioProcessor::onBuildupPeak(function<void()> callback) {
    auto detector = getBuildupDetector();
    detector->onBuildupPeak = callback;
}

void AudioProcessor::onBuildupEnd(function<void()> callback) {
    auto detector = getBuildupDetector();
    detector->onBuildupEnd = callback;
}

void AudioProcessor::onBuildup(function<void(const Buildup&)> callback) {
    auto detector = getBuildupDetector();
    detector->onBuildup = callback;
}

void AudioProcessor::onDrop(function<void()> callback) {
    auto detector = getDropDetector();
    detector->onDrop = callback;
}

void AudioProcessor::onDropEvent(function<void(const Drop&)> callback) {
    auto detector = getDropDetector();
    detector->onDropEvent = callback;
}

void AudioProcessor::onDropImpact(function<void(float)> callback) {
    auto detector = getDropDetector();
    detector->onDropImpact = callback;
}

const AudioSample& AudioProcessor::getSample() const {
    return mContext->getSample();
}

void AudioProcessor::reset() {
    mContext->clearCache();
    if (mBeatDetector) {
        mBeatDetector->reset();
    }
    if (mFrequencyBands) {
        mFrequencyBands->reset();
    }
    if (mEnergyAnalyzer) {
        mEnergyAnalyzer->reset();
    }
    if (mTempoAnalyzer) {
        mTempoAnalyzer->reset();
    }
    if (mTransientDetector) {
        mTransientDetector->reset();
    }
    if (mSilenceDetector) {
        mSilenceDetector->reset();
    }
    if (mDynamicsAnalyzer) {
        mDynamicsAnalyzer->reset();
    }
    if (mPitchDetector) {
        mPitchDetector->reset();
    }
    if (mNoteDetector) {
        mNoteDetector->reset();
    }
    if (mDownbeatDetector) {
        mDownbeatDetector->reset();
    }
    if (mBackbeatDetector) {
        mBackbeatDetector->reset();
    }
    if (mVocalDetector) {
        mVocalDetector->reset();
    }
    if (mPercussionDetector) {
        mPercussionDetector->reset();
    }
    if (mChordDetector) {
        mChordDetector->reset();
    }
    if (mKeyDetector) {
        mKeyDetector->reset();
    }
    if (mMoodAnalyzer) {
        mMoodAnalyzer->reset();
    }
    if (mBuildupDetector) {
        mBuildupDetector->reset();
    }
    if (mDropDetector) {
        mDropDetector->reset();
    }
}

shared_ptr<BeatDetector> AudioProcessor::getBeatDetector() {
    if (!mBeatDetector) {
        mBeatDetector = make_shared<BeatDetector>();
    }
    return mBeatDetector;
}

shared_ptr<FrequencyBands> AudioProcessor::getFrequencyBands() {
    if (!mFrequencyBands) {
        mFrequencyBands = make_shared<FrequencyBands>();
    }
    return mFrequencyBands;
}

shared_ptr<EnergyAnalyzer> AudioProcessor::getEnergyAnalyzer() {
    if (!mEnergyAnalyzer) {
        mEnergyAnalyzer = make_shared<EnergyAnalyzer>();
    }
    return mEnergyAnalyzer;
}

shared_ptr<TempoAnalyzer> AudioProcessor::getTempoAnalyzer() {
    if (!mTempoAnalyzer) {
        mTempoAnalyzer = make_shared<TempoAnalyzer>();
    }
    return mTempoAnalyzer;
}

shared_ptr<TransientDetector> AudioProcessor::getTransientDetector() {
    if (!mTransientDetector) {
        mTransientDetector = make_shared<TransientDetector>();
    }
    return mTransientDetector;
}

shared_ptr<SilenceDetector> AudioProcessor::getSilenceDetector() {
    if (!mSilenceDetector) {
        mSilenceDetector = make_shared<SilenceDetector>();
    }
    return mSilenceDetector;
}

shared_ptr<DynamicsAnalyzer> AudioProcessor::getDynamicsAnalyzer() {
    if (!mDynamicsAnalyzer) {
        mDynamicsAnalyzer = make_shared<DynamicsAnalyzer>();
    }
    return mDynamicsAnalyzer;
}

shared_ptr<PitchDetector> AudioProcessor::getPitchDetector() {
    if (!mPitchDetector) {
        mPitchDetector = make_shared<PitchDetector>();
    }
    return mPitchDetector;
}

shared_ptr<NoteDetector> AudioProcessor::getNoteDetector() {
    if (!mNoteDetector) {
        // Share the PitchDetector instance between AudioProcessor and NoteDetector
        auto pitchDetector = getPitchDetector();
        mNoteDetector = make_shared<NoteDetector>(pitchDetector);
    }
    return mNoteDetector;
}

shared_ptr<DownbeatDetector> AudioProcessor::getDownbeatDetector() {
    if (!mDownbeatDetector) {
        // Share the BeatDetector instance between AudioProcessor and DownbeatDetector
        auto beatDetector = getBeatDetector();
        mDownbeatDetector = make_shared<DownbeatDetector>(beatDetector);
    }
    return mDownbeatDetector;
}

shared_ptr<BackbeatDetector> AudioProcessor::getBackbeatDetector() {
    if (!mBackbeatDetector) {
        // Share the BeatDetector and DownbeatDetector instances with BackbeatDetector
        auto beatDetector = getBeatDetector();
        auto downbeatDetector = getDownbeatDetector();
        mBackbeatDetector = make_shared<BackbeatDetector>(beatDetector, downbeatDetector);
    }
    return mBackbeatDetector;
}

shared_ptr<VocalDetector> AudioProcessor::getVocalDetector() {
    if (!mVocalDetector) {
        mVocalDetector = make_shared<VocalDetector>();
    }
    return mVocalDetector;
}

shared_ptr<PercussionDetector> AudioProcessor::getPercussionDetector() {
    if (!mPercussionDetector) {
        mPercussionDetector = make_shared<PercussionDetector>();
    }
    return mPercussionDetector;
}

shared_ptr<ChordDetector> AudioProcessor::getChordDetector() {
    if (!mChordDetector) {
        mChordDetector = make_shared<ChordDetector>();
    }
    return mChordDetector;
}

shared_ptr<KeyDetector> AudioProcessor::getKeyDetector() {
    if (!mKeyDetector) {
        mKeyDetector = make_shared<KeyDetector>();
    }
    return mKeyDetector;
}

shared_ptr<MoodAnalyzer> AudioProcessor::getMoodAnalyzer() {
    if (!mMoodAnalyzer) {
        mMoodAnalyzer = make_shared<MoodAnalyzer>();
    }
    return mMoodAnalyzer;
}

shared_ptr<BuildupDetector> AudioProcessor::getBuildupDetector() {
    if (!mBuildupDetector) {
        mBuildupDetector = make_shared<BuildupDetector>();
    }
    return mBuildupDetector;
}

shared_ptr<DropDetector> AudioProcessor::getDropDetector() {
    if (!mDropDetector) {
        mDropDetector = make_shared<DropDetector>();
    }
    return mDropDetector;
}

} // namespace fl
