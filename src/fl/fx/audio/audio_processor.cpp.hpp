#include "fl/fx/audio/audio_processor.h"
#include "fl/fx/audio/detectors/beat.h"
#include "fl/fx/audio/detectors/frequency_bands.h"
#include "fl/fx/audio/detectors/energy_analyzer.h"
#include "fl/fx/audio/detectors/tempo_analyzer.h"
#include "fl/fx/audio/detectors/transient.h"
#include "fl/fx/audio/detectors/silence.h"
#include "fl/fx/audio/detectors/dynamics_analyzer.h"
#include "fl/fx/audio/detectors/pitch.h"
#include "fl/fx/audio/detectors/note.h"
#include "fl/fx/audio/detectors/downbeat.h"
#include "fl/fx/audio/detectors/backbeat.h"
#include "fl/fx/audio/detectors/vocal.h"
#include "fl/fx/audio/detectors/percussion.h"
#include "fl/fx/audio/detectors/chord.h"
#include "fl/fx/audio/detectors/key.h"
#include "fl/fx/audio/detectors/mood_analyzer.h"
#include "fl/fx/audio/detectors/buildup.h"
#include "fl/fx/audio/detectors/drop.h"

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

    // Stage 2: Automatic gain control
    if (mAutoGainEnabled && conditioned.isValid()) {
        conditioned = mAutoGain.process(conditioned);
        if (!conditioned.isValid()) {
            return;
        }
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

void AudioProcessor::onPercussion(function<void(const char*)> callback) {
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

// ----- Polling Getter Implementations -----

u8 AudioProcessor::getVocalConfidence() {
    return static_cast<u8>(getVocalDetector()->getConfidence() * 255.0f);
}

u8 AudioProcessor::isVocalActive() {
    return static_cast<u8>(getVocalDetector()->getConfidence() * 255.0f);
}

u8 AudioProcessor::getBeatConfidence() {
    return static_cast<u8>(getBeatDetector()->getConfidence() * 255.0f);
}

u8 AudioProcessor::isBeat() {
    return static_cast<u8>(getBeatDetector()->getConfidence() * 255.0f);
}

float AudioProcessor::getBPM() {
    return getBeatDetector()->getBPM();
}

u8 AudioProcessor::getEnergy() {
    return static_cast<u8>(getEnergyAnalyzer()->getNormalizedRMS() * 255.0f);
}

u8 AudioProcessor::getPeakLevel() {
    float peak = getEnergyAnalyzer()->getPeak();
    // Clamp to 0..1 range then scale
    if (peak < 0.0f) peak = 0.0f;
    if (peak > 1.0f) peak = 1.0f;
    return static_cast<u8>(peak * 255.0f);
}

u8 AudioProcessor::getBassLevel() {
    float bass = getFrequencyBands()->getBass();
    if (bass < 0.0f) bass = 0.0f;
    if (bass > 1.0f) bass = 1.0f;
    return static_cast<u8>(bass * 255.0f);
}

u8 AudioProcessor::getMidLevel() {
    float mid = getFrequencyBands()->getMid();
    if (mid < 0.0f) mid = 0.0f;
    if (mid > 1.0f) mid = 1.0f;
    return static_cast<u8>(mid * 255.0f);
}

u8 AudioProcessor::getTrebleLevel() {
    float treble = getFrequencyBands()->getTreble();
    if (treble < 0.0f) treble = 0.0f;
    if (treble > 1.0f) treble = 1.0f;
    return static_cast<u8>(treble * 255.0f);
}

u8 AudioProcessor::isSilent() {
    return getSilenceDetector()->isSilent() ? 255 : 0;
}

u32 AudioProcessor::getSilenceDuration() {
    return getSilenceDetector()->getSilenceDuration();
}

u8 AudioProcessor::getTransientStrength() {
    float strength = getTransientDetector()->getStrength();
    if (strength < 0.0f) strength = 0.0f;
    if (strength > 1.0f) strength = 1.0f;
    return static_cast<u8>(strength * 255.0f);
}

u8 AudioProcessor::isTransient() {
    float strength = getTransientDetector()->getStrength();
    if (strength < 0.0f) strength = 0.0f;
    if (strength > 1.0f) strength = 1.0f;
    return static_cast<u8>(strength * 255.0f);
}

u8 AudioProcessor::getDynamicTrend() {
    // Maps -1..1 to 0..255 (128 = neutral)
    float trend = getDynamicsAnalyzer()->getDynamicTrend();
    if (trend < -1.0f) trend = -1.0f;
    if (trend > 1.0f) trend = 1.0f;
    return static_cast<u8>((trend + 1.0f) * 0.5f * 255.0f);
}

u8 AudioProcessor::isCrescendo() {
    return getDynamicsAnalyzer()->isCrescendo() ? 255 : 0;
}

u8 AudioProcessor::isDiminuendo() {
    return getDynamicsAnalyzer()->isDiminuendo() ? 255 : 0;
}

u8 AudioProcessor::getPitchConfidence() {
    return static_cast<u8>(getPitchDetector()->getConfidence() * 255.0f);
}

float AudioProcessor::getPitch() {
    return getPitchDetector()->getPitch();
}

u8 AudioProcessor::isVoiced() {
    return static_cast<u8>(getPitchDetector()->getConfidence() * 255.0f);
}

u8 AudioProcessor::getTempoConfidence() {
    return static_cast<u8>(getTempoAnalyzer()->getConfidence() * 255.0f);
}

float AudioProcessor::getTempoBPM() {
    return getTempoAnalyzer()->getBPM();
}

u8 AudioProcessor::isTempoStable() {
    return static_cast<u8>(getTempoAnalyzer()->getConfidence() * 255.0f);
}

u8 AudioProcessor::getBuildupIntensity() {
    float intensity = getBuildupDetector()->getIntensity();
    if (intensity < 0.0f) intensity = 0.0f;
    if (intensity > 1.0f) intensity = 1.0f;
    return static_cast<u8>(intensity * 255.0f);
}

u8 AudioProcessor::getBuildupProgress() {
    float progress = getBuildupDetector()->getProgress();
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    return static_cast<u8>(progress * 255.0f);
}

u8 AudioProcessor::isBuilding() {
    float intensity = getBuildupDetector()->getIntensity();
    if (intensity < 0.0f) intensity = 0.0f;
    if (intensity > 1.0f) intensity = 1.0f;
    return static_cast<u8>(intensity * 255.0f);
}

u8 AudioProcessor::getDropImpact() {
    float impact = getDropDetector()->getLastDrop().impact;
    if (impact < 0.0f) impact = 0.0f;
    if (impact > 1.0f) impact = 1.0f;
    return static_cast<u8>(impact * 255.0f);
}

u8 AudioProcessor::isKick() {
    return getPercussionDetector()->isKick() ? 255 : 0;
}

u8 AudioProcessor::isSnare() {
    return getPercussionDetector()->isSnare() ? 255 : 0;
}

u8 AudioProcessor::isHiHat() {
    return getPercussionDetector()->isHiHat() ? 255 : 0;
}

u8 AudioProcessor::isTom() {
    return getPercussionDetector()->isTom() ? 255 : 0;
}

u8 AudioProcessor::getCurrentNote() {
    return getNoteDetector()->getCurrentNote();
}

u8 AudioProcessor::getNoteVelocity() {
    return getNoteDetector()->getLastVelocity();
}

u8 AudioProcessor::isNoteActive() {
    return getNoteDetector()->isNoteActive() ? getNoteDetector()->getLastVelocity() : 0;
}

u8 AudioProcessor::isDownbeat() {
    return static_cast<u8>(getDownbeatDetector()->getConfidence() * 255.0f);
}

u8 AudioProcessor::getMeasurePhase() {
    float phase = getDownbeatDetector()->getMeasurePhase();
    if (phase < 0.0f) phase = 0.0f;
    if (phase > 1.0f) phase = 1.0f;
    return static_cast<u8>(phase * 255.0f);
}

u8 AudioProcessor::getCurrentBeatNumber() {
    return getDownbeatDetector()->getCurrentBeat();
}

u8 AudioProcessor::getBackbeatConfidence() {
    return static_cast<u8>(getBackbeatDetector()->getConfidence() * 255.0f);
}

u8 AudioProcessor::getBackbeatStrength() {
    float strength = getBackbeatDetector()->getStrength();
    if (strength < 0.0f) strength = 0.0f;
    if (strength > 1.0f) strength = 1.0f;
    return static_cast<u8>(strength * 255.0f);
}

u8 AudioProcessor::hasChord() {
    return static_cast<u8>(getChordDetector()->getCurrentChord().confidence * 255.0f);
}

u8 AudioProcessor::getChordConfidence() {
    return static_cast<u8>(getChordDetector()->getCurrentChord().confidence * 255.0f);
}

u8 AudioProcessor::hasKey() {
    return static_cast<u8>(getKeyDetector()->getCurrentKey().confidence * 255.0f);
}

u8 AudioProcessor::getKeyConfidence() {
    return static_cast<u8>(getKeyDetector()->getCurrentKey().confidence * 255.0f);
}

u8 AudioProcessor::getMoodArousal() {
    float arousal = getMoodAnalyzer()->getArousal();
    if (arousal < 0.0f) arousal = 0.0f;
    if (arousal > 1.0f) arousal = 1.0f;
    return static_cast<u8>(arousal * 255.0f);
}

u8 AudioProcessor::getMoodValence() {
    // Maps -1..1 to 0..255 (128 = neutral)
    float valence = getMoodAnalyzer()->getValence();
    if (valence < -1.0f) valence = -1.0f;
    if (valence > 1.0f) valence = 1.0f;
    return static_cast<u8>((valence + 1.0f) * 0.5f * 255.0f);
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

void AudioProcessor::setSignalConditioningEnabled(bool enabled) {
    mSignalConditioningEnabled = enabled;
}

void AudioProcessor::setAutoGainEnabled(bool enabled) {
    mAutoGainEnabled = enabled;
}

void AudioProcessor::setNoiseFloorTrackingEnabled(bool enabled) {
    mNoiseFloorTrackingEnabled = enabled;
}

void AudioProcessor::configureSignalConditioner(const SignalConditionerConfig& config) {
    mSignalConditioner.configure(config);
    mSignalConditioningEnabled = true;
}

void AudioProcessor::configureAutoGain(const AutoGainConfig& config) {
    mAutoGain.configure(config);
    mAutoGainEnabled = config.enabled;
}

void AudioProcessor::configureNoiseFloorTracker(const NoiseFloorTrackerConfig& config) {
    mNoiseFloorTracker.configure(config);
    mNoiseFloorTrackingEnabled = config.enabled;
}

const AudioSample& AudioProcessor::getSample() const {
    return mContext->getSample();
}

void AudioProcessor::reset() {
    mSignalConditioner.reset();
    mAutoGain.reset();
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

} // namespace fl
