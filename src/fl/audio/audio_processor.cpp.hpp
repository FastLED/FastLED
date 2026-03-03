#include "fl/audio/audio_processor.h"
#include "fl/audio/input.h"
#include "fl/audio/detectors/beat.h"
#include "fl/audio/detectors/frequency_bands.h"
#include "fl/audio/detectors/energy_analyzer.h"
#include "fl/audio/detectors/tempo_analyzer.h"
#include "fl/audio/detectors/transient.h"
#include "fl/audio/detectors/silence.h"
#include "fl/audio/detectors/dynamics_analyzer.h"
#include "fl/audio/detectors/pitch.h"
#include "fl/audio/detectors/note.h"
#include "fl/audio/detectors/downbeat.h"
#include "fl/audio/detectors/backbeat.h"
#include "fl/audio/detectors/vocal.h"
#include "fl/audio/detectors/percussion.h"
#include "fl/audio/detectors/chord.h"
#include "fl/audio/detectors/key.h"
#include "fl/audio/detectors/mood_analyzer.h"
#include "fl/audio/detectors/buildup.h"
#include "fl/audio/detectors/drop.h"
#include "fl/audio/detectors/equalizer.h"
#include "fl/audio/detectors/vibe.h"

namespace fl {

AudioProcessor::AudioProcessor()
    : mContext(make_shared<AudioContext>(AudioSample()))
{}

void AudioProcessor::registerDetector(shared_ptr<AudioDetector> detector) {
    mActiveDetectors.push_back(detector);
}

AudioProcessor::~AudioProcessor() = default;

void AudioProcessor::update(const AudioSample& sample) {
    // Signal conditioning pipeline: raw sample → conditioned sample
    AudioSample conditioned = sample;

    // Stage 1: Signal conditioning (DC removal, spike filtering, noise gate)
    if (mSignalConditioningEnabled && conditioned.isValid()) {
        conditioned = mSignalConditioner.processSample(conditioned);
        if (!conditioned.isValid()) {
            return;  // Signal was entirely filtered out
        }
    }

    // Stage 2: Digital gain (simple multiplier)
    if (mGain != 1.0f && conditioned.isValid()) {
        conditioned.applyGain(mGain);
    }

    // Stage 3: Noise floor tracking (passive — updates estimate but doesn't modify signal)
    if (mNoiseFloorTrackingEnabled && conditioned.isValid()) {
        mNoiseFloorTracker.update(conditioned.rms());
    }

    mContext->setSample(conditioned);

    // Phase 1: Compute state for all active detectors
    for (auto& d : mActiveDetectors) {
        d->update(mContext);
    }

    // Phase 2: Fire callbacks for all active detectors
    for (auto& d : mActiveDetectors) {
        d->fireCallbacks();
    }
}

void AudioProcessor::onBeat(function<void()> callback) {
    auto detector = getBeatDetector();
    detector->onBeat.add(callback);
}

void AudioProcessor::onBeatPhase(function<void(float)> callback) {
    auto detector = getBeatDetector();
    detector->onBeatPhase.add(callback);
}

void AudioProcessor::onOnset(function<void(float)> callback) {
    auto detector = getBeatDetector();
    detector->onOnset.add(callback);
}

void AudioProcessor::onTempoChange(function<void(float, float)> callback) {
    auto detector = getBeatDetector();
    detector->onTempoChange.add(callback);
}

void AudioProcessor::onTempo(function<void(float)> callback) {
    auto detector = getTempoAnalyzer();
    detector->onTempo.add(callback);
}

void AudioProcessor::onTempoWithConfidence(function<void(float, float)> callback) {
    auto detector = getTempoAnalyzer();
    detector->onTempoWithConfidence.add(callback);
}

void AudioProcessor::onTempoStable(function<void()> callback) {
    auto detector = getTempoAnalyzer();
    detector->onTempoStable.add(callback);
}

void AudioProcessor::onTempoUnstable(function<void()> callback) {
    auto detector = getTempoAnalyzer();
    detector->onTempoUnstable.add(callback);
}

void AudioProcessor::onBass(function<void(float)> callback) {
    auto detector = getFrequencyBands();
    detector->onBassLevel.add(callback);
}

void AudioProcessor::onMid(function<void(float)> callback) {
    auto detector = getFrequencyBands();
    detector->onMidLevel.add(callback);
}

void AudioProcessor::onTreble(function<void(float)> callback) {
    auto detector = getFrequencyBands();
    detector->onTrebleLevel.add(callback);
}

void AudioProcessor::onFrequencyBands(function<void(float, float, float)> callback) {
    auto detector = getFrequencyBands();
    detector->onLevelsUpdate.add(callback);
}

void AudioProcessor::onEqualizer(function<void(const Equalizer&)> callback) {
    auto detector = getEqualizerDetector();
    detector->onEqualizer.add(callback);
}

void AudioProcessor::onEnergy(function<void(float)> callback) {
    auto detector = getEnergyAnalyzer();
    detector->onEnergy.add(callback);
}

void AudioProcessor::onNormalizedEnergy(function<void(float)> callback) {
    auto detector = getEnergyAnalyzer();
    detector->onNormalizedEnergy.add(callback);
}

void AudioProcessor::onPeak(function<void(float)> callback) {
    auto detector = getEnergyAnalyzer();
    detector->onPeak.add(callback);
}

void AudioProcessor::onAverageEnergy(function<void(float)> callback) {
    auto detector = getEnergyAnalyzer();
    detector->onAverageEnergy.add(callback);
}

void AudioProcessor::onTransient(function<void()> callback) {
    auto detector = getTransientDetector();
    detector->onTransient.add(callback);
}

void AudioProcessor::onTransientWithStrength(function<void(float)> callback) {
    auto detector = getTransientDetector();
    detector->onTransientWithStrength.add(callback);
}

void AudioProcessor::onAttack(function<void(float)> callback) {
    auto detector = getTransientDetector();
    detector->onAttack.add(callback);
}

void AudioProcessor::onSilence(function<void(u8)> callback) {
    auto detector = getSilenceDetector();
    detector->onSilence.add(callback);
}

void AudioProcessor::onSilenceStart(function<void()> callback) {
    auto detector = getSilenceDetector();
    detector->onSilenceStart.add(callback);
}

void AudioProcessor::onSilenceEnd(function<void()> callback) {
    auto detector = getSilenceDetector();
    detector->onSilenceEnd.add(callback);
}

void AudioProcessor::onSilenceDuration(function<void(u32)> callback) {
    auto detector = getSilenceDetector();
    detector->onSilenceDuration.add(callback);
}

void AudioProcessor::onCrescendo(function<void()> callback) {
    auto detector = getDynamicsAnalyzer();
    detector->onCrescendo.add(callback);
}

void AudioProcessor::onDiminuendo(function<void()> callback) {
    auto detector = getDynamicsAnalyzer();
    detector->onDiminuendo.add(callback);
}

void AudioProcessor::onDynamicTrend(function<void(float)> callback) {
    auto detector = getDynamicsAnalyzer();
    detector->onDynamicTrend.add(callback);
}

void AudioProcessor::onCompressionRatio(function<void(float)> callback) {
    auto detector = getDynamicsAnalyzer();
    detector->onCompressionRatio.add(callback);
}

void AudioProcessor::onPitch(function<void(float)> callback) {
    auto detector = getPitchDetector();
    detector->onPitch.add(callback);
}

void AudioProcessor::onPitchWithConfidence(function<void(float, float)> callback) {
    auto detector = getPitchDetector();
    detector->onPitchWithConfidence.add(callback);
}

void AudioProcessor::onPitchChange(function<void(float)> callback) {
    auto detector = getPitchDetector();
    detector->onPitchChange.add(callback);
}

void AudioProcessor::onVoiced(function<void(u8)> callback) {
    auto detector = getPitchDetector();
    detector->onVoiced.add(callback);
}

void AudioProcessor::onNoteOn(function<void(u8, u8)> callback) {
    auto detector = getNoteDetector();
    detector->onNoteOn.add(callback);
}

void AudioProcessor::onNoteOff(function<void(u8)> callback) {
    auto detector = getNoteDetector();
    detector->onNoteOff.add(callback);
}

void AudioProcessor::onNoteChange(function<void(u8, u8)> callback) {
    auto detector = getNoteDetector();
    detector->onNoteChange.add(callback);
}

void AudioProcessor::onDownbeat(function<void()> callback) {
    auto detector = getDownbeatDetector();
    detector->onDownbeat.add(callback);
}

void AudioProcessor::onMeasureBeat(function<void(u8)> callback) {
    auto detector = getDownbeatDetector();
    detector->onMeasureBeat.add(callback);
}

void AudioProcessor::onMeterChange(function<void(u8)> callback) {
    auto detector = getDownbeatDetector();
    detector->onMeterChange.add(callback);
}

void AudioProcessor::onMeasurePhase(function<void(float)> callback) {
    auto detector = getDownbeatDetector();
    detector->onMeasurePhase.add(callback);
}

void AudioProcessor::onBackbeat(function<void(u8 beatNumber, float confidence, float strength)> callback) {
    auto detector = getBackbeatDetector();
    detector->onBackbeat.add(callback);
}

void AudioProcessor::onVocal(function<void(u8)> callback) {
    auto detector = getVocalDetector();
    detector->onVocal.add(callback);
}

void AudioProcessor::onVocalStart(function<void()> callback) {
    auto detector = getVocalDetector();
    detector->onVocalStart.add(callback);
}

void AudioProcessor::onVocalEnd(function<void()> callback) {
    auto detector = getVocalDetector();
    detector->onVocalEnd.add(callback);
}

void AudioProcessor::onVocalConfidence(function<void(float)> callback) {
    auto detector = getVocalDetector();
    // This callback fires every frame with the current confidence
    // We need to wrap it since VocalDetector doesn't have this callback built-in
    detector->onVocal.add([callback, detector](u8) {
        if (callback) {
            callback(detector->getConfidence());
        }
    });
}

void AudioProcessor::onPercussion(function<void(PercussionType)> callback) {
    auto detector = getPercussionDetector();
    detector->onPercussionHit.add(callback);
}

void AudioProcessor::onKick(function<void()> callback) {
    auto detector = getPercussionDetector();
    detector->onKick.add(callback);
}

void AudioProcessor::onSnare(function<void()> callback) {
    auto detector = getPercussionDetector();
    detector->onSnare.add(callback);
}

void AudioProcessor::onHiHat(function<void()> callback) {
    auto detector = getPercussionDetector();
    detector->onHiHat.add(callback);
}

void AudioProcessor::onTom(function<void()> callback) {
    auto detector = getPercussionDetector();
    detector->onTom.add(callback);
}

void AudioProcessor::onChord(function<void(const Chord&)> callback) {
    auto detector = getChordDetector();
    detector->onChord.add(callback);
}

void AudioProcessor::onChordChange(function<void(const Chord&)> callback) {
    auto detector = getChordDetector();
    detector->onChordChange.add(callback);
}

void AudioProcessor::onChordEnd(function<void()> callback) {
    auto detector = getChordDetector();
    detector->onChordEnd.add(callback);
}

void AudioProcessor::onKey(function<void(const Key&)> callback) {
    auto detector = getKeyDetector();
    detector->onKey.add(callback);
}

void AudioProcessor::onKeyChange(function<void(const Key&)> callback) {
    auto detector = getKeyDetector();
    detector->onKeyChange.add(callback);
}

void AudioProcessor::onKeyEnd(function<void()> callback) {
    auto detector = getKeyDetector();
    detector->onKeyEnd.add(callback);
}

void AudioProcessor::onMood(function<void(const Mood&)> callback) {
    auto detector = getMoodAnalyzer();
    detector->onMood.add(callback);
}

void AudioProcessor::onMoodChange(function<void(const Mood&)> callback) {
    auto detector = getMoodAnalyzer();
    detector->onMoodChange.add(callback);
}

void AudioProcessor::onValenceArousal(function<void(float, float)> callback) {
    auto detector = getMoodAnalyzer();
    detector->onValenceArousal.add(callback);
}

void AudioProcessor::onBuildupStart(function<void()> callback) {
    auto detector = getBuildupDetector();
    detector->onBuildupStart.add(callback);
}

void AudioProcessor::onBuildupProgress(function<void(float)> callback) {
    auto detector = getBuildupDetector();
    detector->onBuildupProgress.add(callback);
}

void AudioProcessor::onBuildupPeak(function<void()> callback) {
    auto detector = getBuildupDetector();
    detector->onBuildupPeak.add(callback);
}

void AudioProcessor::onBuildupEnd(function<void()> callback) {
    auto detector = getBuildupDetector();
    detector->onBuildupEnd.add(callback);
}

void AudioProcessor::onBuildup(function<void(const Buildup&)> callback) {
    auto detector = getBuildupDetector();
    detector->onBuildup.add(callback);
}

void AudioProcessor::onDrop(function<void()> callback) {
    auto detector = getDropDetector();
    detector->onDrop.add(callback);
}

void AudioProcessor::onDropEvent(function<void(const Drop&)> callback) {
    auto detector = getDropDetector();
    detector->onDropEvent.add(callback);
}

void AudioProcessor::onDropImpact(function<void(float)> callback) {
    auto detector = getDropDetector();
    detector->onDropImpact.add(callback);
}

void AudioProcessor::onVibeLevels(function<void(const VibeLevels&)> callback) {
    auto detector = getVibeDetector();
    detector->onVibeLevels.add(callback);
}

void AudioProcessor::onVibeBassSpike(function<void()> callback) {
    auto detector = getVibeDetector();
    detector->onBassSpike.add(callback);
}

void AudioProcessor::onVibeMidSpike(function<void()> callback) {
    auto detector = getVibeDetector();
    detector->onMidSpike.add(callback);
}

void AudioProcessor::onVibeTrebSpike(function<void()> callback) {
    auto detector = getVibeDetector();
    detector->onTrebSpike.add(callback);
}

// ----- Polling Getter Implementations -----

// Helper: clamp float to 0..1
static float clamp01(float v) {
    if (v < 0.0f) return 0.0f;
    if (v > 1.0f) return 1.0f;
    return v;
}

// Helper: clamp float to -1..1
static float clampNeg1To1(float v) {
    if (v < -1.0f) return -1.0f;
    if (v > 1.0f) return 1.0f;
    return v;
}

float AudioProcessor::getVocalConfidence() {
    return clamp01(getVocalDetector()->getConfidence());
}

float AudioProcessor::getBeatConfidence() {
    return clamp01(getBeatDetector()->getConfidence());
}

float AudioProcessor::getBPM() {
    return getBeatDetector()->getBPM();
}

float AudioProcessor::getEnergy() {
    return clamp01(getEnergyAnalyzer()->getNormalizedRMS());
}

float AudioProcessor::getPeakLevel() {
    return clamp01(getEnergyAnalyzer()->getPeak());
}

float AudioProcessor::getBassLevel() {
    return clamp01(getFrequencyBands()->getBassNorm());
}

float AudioProcessor::getMidLevel() {
    return clamp01(getFrequencyBands()->getMidNorm());
}

float AudioProcessor::getTrebleLevel() {
    return clamp01(getFrequencyBands()->getTrebleNorm());
}

float AudioProcessor::getBassRaw() {
    return getFrequencyBands()->getBass();
}

float AudioProcessor::getMidRaw() {
    return getFrequencyBands()->getMid();
}

float AudioProcessor::getTrebleRaw() {
    return getFrequencyBands()->getTreble();
}

bool AudioProcessor::isSilent() {
    return getSilenceDetector()->isSilent();
}

u32 AudioProcessor::getSilenceDuration() {
    return getSilenceDetector()->getSilenceDuration();
}

float AudioProcessor::getTransientStrength() {
    return clamp01(getTransientDetector()->getStrength());
}

float AudioProcessor::getDynamicTrend() {
    return clampNeg1To1(getDynamicsAnalyzer()->getDynamicTrend());
}

bool AudioProcessor::isCrescendo() {
    return getDynamicsAnalyzer()->isCrescendo();
}

bool AudioProcessor::isDiminuendo() {
    return getDynamicsAnalyzer()->isDiminuendo();
}

float AudioProcessor::getPitchConfidence() {
    return clamp01(getPitchDetector()->getConfidence());
}

float AudioProcessor::getPitch() {
    return getPitchDetector()->getPitch();
}

float AudioProcessor::getTempoConfidence() {
    return clamp01(getTempoAnalyzer()->getConfidence());
}

float AudioProcessor::getTempoBPM() {
    return getTempoAnalyzer()->getBPM();
}

float AudioProcessor::getBuildupIntensity() {
    return clamp01(getBuildupDetector()->getIntensity());
}

float AudioProcessor::getBuildupProgress() {
    return clamp01(getBuildupDetector()->getProgress());
}

float AudioProcessor::getDropImpact() {
    return clamp01(getDropDetector()->getLastDrop().impact);
}

bool AudioProcessor::isKick() {
    return getPercussionDetector()->isKick();
}

bool AudioProcessor::isSnare() {
    return getPercussionDetector()->isSnare();
}

bool AudioProcessor::isHiHat() {
    return getPercussionDetector()->isHiHat();
}

bool AudioProcessor::isTom() {
    return getPercussionDetector()->isTom();
}

u8 AudioProcessor::getCurrentNote() {
    return getNoteDetector()->getCurrentNote();
}

float AudioProcessor::getNoteVelocity() {
    return getNoteDetector()->getLastVelocity() / 255.0f;
}

float AudioProcessor::getNoteConfidence() {
    return getNoteDetector()->isNoteActive() ? (getNoteDetector()->getLastVelocity() / 255.0f) : 0.0f;
}

float AudioProcessor::getDownbeatConfidence() {
    return clamp01(getDownbeatDetector()->getConfidence());
}

float AudioProcessor::getMeasurePhase() {
    return clamp01(getDownbeatDetector()->getMeasurePhase());
}

u8 AudioProcessor::getCurrentBeatNumber() {
    return getDownbeatDetector()->getCurrentBeat();
}

float AudioProcessor::getBackbeatConfidence() {
    return clamp01(getBackbeatDetector()->getConfidence());
}

float AudioProcessor::getBackbeatStrength() {
    return clamp01(getBackbeatDetector()->getStrength());
}

float AudioProcessor::getChordConfidence() {
    return clamp01(getChordDetector()->getCurrentChord().confidence);
}

float AudioProcessor::getKeyConfidence() {
    return clamp01(getKeyDetector()->getCurrentKey().confidence);
}

float AudioProcessor::getMoodArousal() {
    return clamp01(getMoodAnalyzer()->getArousal());
}

float AudioProcessor::getMoodValence() {
    return clampNeg1To1(getMoodAnalyzer()->getValence());
}

float AudioProcessor::getVibeBass() {
    return getVibeDetector()->getBass();
}

float AudioProcessor::getVibeMid() {
    return getVibeDetector()->getMid();
}

float AudioProcessor::getVibeTreb() {
    return getVibeDetector()->getTreb();
}

float AudioProcessor::getVibeVol() {
    return getVibeDetector()->getVol();
}

float AudioProcessor::getVibeBassAtt() {
    return getVibeDetector()->getBassAtt();
}

float AudioProcessor::getVibeMidAtt() {
    return getVibeDetector()->getMidAtt();
}

float AudioProcessor::getVibeTrebAtt() {
    return getVibeDetector()->getTrebAtt();
}

float AudioProcessor::getVibeVolAtt() {
    return getVibeDetector()->getVolAtt();
}

bool AudioProcessor::isVibeBassSpike() {
    return getVibeDetector()->isBassSpike();
}

bool AudioProcessor::isVibeMidSpike() {
    return getVibeDetector()->isMidSpike();
}

bool AudioProcessor::isVibeTrebSpike() {
    return getVibeDetector()->isTrebSpike();
}

float AudioProcessor::getEqBass() {
    return clamp01(getEqualizerDetector()->getBass());
}

float AudioProcessor::getEqMid() {
    return clamp01(getEqualizerDetector()->getMid());
}

float AudioProcessor::getEqTreble() {
    return clamp01(getEqualizerDetector()->getTreble());
}

float AudioProcessor::getEqVolume() {
    return clamp01(getEqualizerDetector()->getVolume());
}

float AudioProcessor::getEqZcf() {
    return clamp01(getEqualizerDetector()->getZcf());
}

float AudioProcessor::getEqBin(int index) {
    return clamp01(getEqualizerDetector()->getBin(index));
}

float AudioProcessor::getEqAutoGain() {
    return getEqualizerDetector()->getAutoGain();
}

bool AudioProcessor::getEqIsSilence() {
    return getEqualizerDetector()->getIsSilence();
}

float AudioProcessor::getEqDominantFreqHz() {
    return getEqualizerDetector()->getDominantFreqHz();
}

float AudioProcessor::getEqDominantMagnitude() {
    return clamp01(getEqualizerDetector()->getDominantMagnitude());
}

float AudioProcessor::getEqVolumeDb() {
    return getEqualizerDetector()->getVolumeDb();
}

void AudioProcessor::setSampleRate(int sampleRate) {
    mSampleRate = sampleRate;
    mContext->setSampleRate(sampleRate);

    // Propagate to all active detectors that are sample-rate-aware
    for (auto& d : mActiveDetectors) {
        d->setSampleRate(sampleRate);
    }
}

int AudioProcessor::getSampleRate() const {
    return mSampleRate;
}

void AudioProcessor::setGain(float gain) {
    mGain = gain;
}

float AudioProcessor::getGain() const {
    return mGain;
}

void AudioProcessor::setSignalConditioningEnabled(bool enabled) {
    mSignalConditioningEnabled = enabled;
}

void AudioProcessor::setNoiseFloorTrackingEnabled(bool enabled) {
    mNoiseFloorTrackingEnabled = enabled;
}

void AudioProcessor::configureSignalConditioner(const SignalConditionerConfig& config) {
    mSignalConditioner.configure(config);
    mSignalConditioningEnabled = true;
}

void AudioProcessor::configureNoiseFloorTracker(const NoiseFloorTrackerConfig& config) {
    mNoiseFloorTracker.configure(config);
    mNoiseFloorTrackingEnabled = config.enabled;
}

void AudioProcessor::setMicProfile(MicProfile profile) {
    mMicProfile = profile;
    if (mEqualizerDetector) {
        mEqualizerDetector->setMicProfile(profile);
    }
}

void AudioProcessor::configureEqualizer(const EqualizerConfig& config) {
    getEqualizerDetector()->configure(config);
}

const AudioSample& AudioProcessor::getSample() const {
    return mContext->getSample();
}

void AudioProcessor::reset() {
    mSignalConditioner.reset();
    mNoiseFloorTracker.reset();
    mContext->clearCache();

    for (auto& d : mActiveDetectors) {
        d->reset();
    }
    mActiveDetectors.clear();

    // Null out all typed pointers so re-registration works on next use
    mBeatDetector.reset();
    mFrequencyBands.reset();
    mEnergyAnalyzer.reset();
    mTempoAnalyzer.reset();
    mTransientDetector.reset();
    mSilenceDetector.reset();
    mDynamicsAnalyzer.reset();
    mPitchDetector.reset();
    mNoteDetector.reset();
    mDownbeatDetector.reset();
    mBackbeatDetector.reset();
    mVocalDetector.reset();
    mPercussionDetector.reset();
    mChordDetector.reset();
    mKeyDetector.reset();
    mMoodAnalyzer.reset();
    mBuildupDetector.reset();
    mDropDetector.reset();
    mEqualizerDetector.reset();
    mVibeDetector.reset();
}

shared_ptr<BeatDetector> AudioProcessor::getBeatDetector() {
    if (!mBeatDetector) {
        mBeatDetector = make_shared<BeatDetector>();
        registerDetector(mBeatDetector);
    }
    return mBeatDetector;
}

shared_ptr<FrequencyBands> AudioProcessor::getFrequencyBands() {
    if (!mFrequencyBands) {
        mFrequencyBands = make_shared<FrequencyBands>();
        registerDetector(mFrequencyBands);
    }
    return mFrequencyBands;
}

shared_ptr<EnergyAnalyzer> AudioProcessor::getEnergyAnalyzer() {
    if (!mEnergyAnalyzer) {
        mEnergyAnalyzer = make_shared<EnergyAnalyzer>();
        registerDetector(mEnergyAnalyzer);
    }
    return mEnergyAnalyzer;
}

shared_ptr<TempoAnalyzer> AudioProcessor::getTempoAnalyzer() {
    if (!mTempoAnalyzer) {
        mTempoAnalyzer = make_shared<TempoAnalyzer>();
        registerDetector(mTempoAnalyzer);
    }
    return mTempoAnalyzer;
}

shared_ptr<TransientDetector> AudioProcessor::getTransientDetector() {
    if (!mTransientDetector) {
        mTransientDetector = make_shared<TransientDetector>();
        registerDetector(mTransientDetector);
    }
    return mTransientDetector;
}

shared_ptr<SilenceDetector> AudioProcessor::getSilenceDetector() {
    if (!mSilenceDetector) {
        mSilenceDetector = make_shared<SilenceDetector>();
        registerDetector(mSilenceDetector);
    }
    return mSilenceDetector;
}

shared_ptr<DynamicsAnalyzer> AudioProcessor::getDynamicsAnalyzer() {
    if (!mDynamicsAnalyzer) {
        mDynamicsAnalyzer = make_shared<DynamicsAnalyzer>();
        registerDetector(mDynamicsAnalyzer);
    }
    return mDynamicsAnalyzer;
}

shared_ptr<PitchDetector> AudioProcessor::getPitchDetector() {
    if (!mPitchDetector) {
        mPitchDetector = make_shared<PitchDetector>();
        registerDetector(mPitchDetector);
    }
    return mPitchDetector;
}

shared_ptr<NoteDetector> AudioProcessor::getNoteDetector() {
    if (!mNoteDetector) {
        // Share the PitchDetector instance between AudioProcessor and NoteDetector
        auto pitchDetector = getPitchDetector();
        mNoteDetector = make_shared<NoteDetector>(pitchDetector);
        registerDetector(mNoteDetector);
    }
    return mNoteDetector;
}

shared_ptr<DownbeatDetector> AudioProcessor::getDownbeatDetector() {
    if (!mDownbeatDetector) {
        // Share the BeatDetector instance between AudioProcessor and DownbeatDetector
        auto beatDetector = getBeatDetector();
        mDownbeatDetector = make_shared<DownbeatDetector>(beatDetector);
        registerDetector(mDownbeatDetector);
    }
    return mDownbeatDetector;
}

shared_ptr<BackbeatDetector> AudioProcessor::getBackbeatDetector() {
    if (!mBackbeatDetector) {
        // Share the BeatDetector and DownbeatDetector instances with BackbeatDetector
        auto beatDetector = getBeatDetector();
        auto downbeatDetector = getDownbeatDetector();
        mBackbeatDetector = make_shared<BackbeatDetector>(beatDetector, downbeatDetector);
        registerDetector(mBackbeatDetector);
    }
    return mBackbeatDetector;
}

shared_ptr<VocalDetector> AudioProcessor::getVocalDetector() {
    if (!mVocalDetector) {
        mVocalDetector = make_shared<VocalDetector>();
        registerDetector(mVocalDetector);
    }
    return mVocalDetector;
}

shared_ptr<PercussionDetector> AudioProcessor::getPercussionDetector() {
    if (!mPercussionDetector) {
        mPercussionDetector = make_shared<PercussionDetector>();
        registerDetector(mPercussionDetector);
    }
    return mPercussionDetector;
}

shared_ptr<ChordDetector> AudioProcessor::getChordDetector() {
    if (!mChordDetector) {
        mChordDetector = make_shared<ChordDetector>();
        registerDetector(mChordDetector);
    }
    return mChordDetector;
}

shared_ptr<KeyDetector> AudioProcessor::getKeyDetector() {
    if (!mKeyDetector) {
        mKeyDetector = make_shared<KeyDetector>();
        registerDetector(mKeyDetector);
    }
    return mKeyDetector;
}

shared_ptr<MoodAnalyzer> AudioProcessor::getMoodAnalyzer() {
    if (!mMoodAnalyzer) {
        mMoodAnalyzer = make_shared<MoodAnalyzer>();
        registerDetector(mMoodAnalyzer);
    }
    return mMoodAnalyzer;
}

shared_ptr<BuildupDetector> AudioProcessor::getBuildupDetector() {
    if (!mBuildupDetector) {
        mBuildupDetector = make_shared<BuildupDetector>();
        registerDetector(mBuildupDetector);
    }
    return mBuildupDetector;
}

shared_ptr<DropDetector> AudioProcessor::getDropDetector() {
    if (!mDropDetector) {
        mDropDetector = make_shared<DropDetector>();
        registerDetector(mDropDetector);
    }
    return mDropDetector;
}

shared_ptr<EqualizerDetector> AudioProcessor::getEqualizerDetector() {
    if (!mEqualizerDetector) {
        mEqualizerDetector = make_shared<EqualizerDetector>();
        if (mMicProfile != MicProfile::None) {
            mEqualizerDetector->setMicProfile(mMicProfile);
        }
        registerDetector(mEqualizerDetector);
    }
    return mEqualizerDetector;
}

shared_ptr<VibeDetector> AudioProcessor::getVibeDetector() {
    if (!mVibeDetector) {
        mVibeDetector = make_shared<VibeDetector>();
        registerDetector(mVibeDetector);
    }
    return mVibeDetector;
}

shared_ptr<AudioProcessor> AudioProcessor::createWithAutoInput(
        shared_ptr<IAudioInput> input) {
    auto processor = make_shared<AudioProcessor>();
    processor->mAudioInput = input;
    // weak_ptr so the lambda doesn't prevent AudioProcessor destruction
    weak_ptr<AudioProcessor> weak = processor;
    weak_ptr<IAudioInput> weakInput = input;
    processor->mAutoTask = fl::task::every_ms(1).then([weak, weakInput]() {
        auto proc = weak.lock();
        auto inp = weakInput.lock();
        if (!proc || !inp) {
            return;
        }
        vector_inlined<AudioSample, 16> samples;
        inp->readAll(&samples);
        for (const auto& sample : samples) {
            proc->update(sample);
        }
    });
    return processor;
}

} // namespace fl
