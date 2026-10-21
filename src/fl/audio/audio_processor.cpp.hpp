#include "fl/audio/audio_processor.h"
#include "fl/stl/weak_ptr.h"
#include "fl/audio/input.h"
#include "fl/audio/detector/beat.h"
#include "fl/audio/detector/frequency_bands.h"
#include "fl/audio/detector/energy_analyzer.h"
#include "fl/audio/detector/tempo_analyzer.h"
#include "fl/audio/detector/transient.h"
#include "fl/audio/detector/silence.h"
#include "fl/audio/detector/dynamics_analyzer.h"
#include "fl/audio/detector/pitch.h"
#include "fl/audio/detector/note.h"
#include "fl/audio/detector/downbeat.h"
#include "fl/audio/detector/backbeat.h"
#include "fl/audio/detector/vocal.h"
#include "fl/audio/detector/percussion.h"
#include "fl/audio/detector/chord.h"
#include "fl/audio/detector/key.h"
#include "fl/audio/detector/mood_analyzer.h"
#include "fl/audio/detector/buildup.h"
#include "fl/audio/detector/drop.h"
#include "fl/audio/detector/equalizer.h"
#include "fl/audio/detector/vibe.h"
#include "fl/stl/noexcept.h"

namespace fl {
namespace audio {

Processor::Processor()
    FL_NO_EXCEPT : mContext(make_shared<Context>(Sample()))
{}

void Processor::registerDetector(shared_ptr<Detector> detector) FL_NO_EXCEPT {
    mActiveDetectors.push_back(detector);
}

Processor::~Processor() FL_NO_EXCEPT = default;

void Processor::update(const Sample& sample) FL_NO_EXCEPT {
    // Signal conditioning pipeline: raw sample → conditioned sample
    Sample conditioned = sample;

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

    // Stage 4: Silence flag — detectors use this to gate outputs when audio stops.
    // Only populated when NFT is enabled; otherwise silence is unknowable and
    // stays at its default (false) after setSample() above.
    //
    // Uses an absolute RMS threshold rather than NFT.isAboveFloor(): the
    // adaptive floor's first-sample init matches the current level, so
    // isAboveFloor() stays false for any steady signal. Absolute RMS is the
    // right silence primitive — a loud constant tone has large RMS.
    if (mNoiseFloorTrackingEnabled && conditioned.isValid()) {
        constexpr float kSilenceRmsThreshold = 10.0f;
        mContext->setSilent(conditioned.rms() < kSilenceRmsThreshold);
    }

    // Phase 1: Compute state for all active detector
    for (auto& d : mActiveDetectors) {
        d->update(mContext);
    }

    // Phase 2: Fire callbacks for all active detector
    for (auto& d : mActiveDetectors) {
        d->fireCallbacks();
    }
}

void Processor::updateFromContext(shared_ptr<Context> externalContext) FL_NO_EXCEPT {
    // Use externally-provided context (FFT already cached, signal already conditioned).
    // This avoids recomputing FFT when Reactive has already done it.
    for (auto& d : mActiveDetectors) {
        d->update(externalContext);
    }
    for (auto& d : mActiveDetectors) {
        d->fireCallbacks();
    }
}

void Processor::onBeat(function<void()> callback) FL_NO_EXCEPT {
    auto detector = getBeatDetector();
    detector->onBeat.add(callback);
}

void Processor::onBeatPhase(function<void(float)> callback) FL_NO_EXCEPT {
    auto detector = getBeatDetector();
    detector->onBeatPhase.add(callback);
}

void Processor::onOnset(function<void(float)> callback) FL_NO_EXCEPT {
    auto detector = getBeatDetector();
    detector->onOnset.add(callback);
}

void Processor::onTempoChange(function<void(float, float)> callback) FL_NO_EXCEPT {
    auto detector = getBeatDetector();
    detector->onTempoChange.add(callback);
}

void Processor::onTempo(function<void(float)> callback) FL_NO_EXCEPT {
    auto detector = getTempoAnalyzer();
    detector->onTempo.add(callback);
}

void Processor::onTempoWithConfidence(function<void(float, float)> callback) FL_NO_EXCEPT {
    auto detector = getTempoAnalyzer();
    detector->onTempoWithConfidence.add(callback);
}

void Processor::onTempoStable(function<void()> callback) FL_NO_EXCEPT {
    auto detector = getTempoAnalyzer();
    detector->onTempoStable.add(callback);
}

void Processor::onTempoUnstable(function<void()> callback) FL_NO_EXCEPT {
    auto detector = getTempoAnalyzer();
    detector->onTempoUnstable.add(callback);
}

void Processor::onBass(function<void(float)> callback) FL_NO_EXCEPT {
    auto detector = getFrequencyBands();
    detector->onBassLevel.add(callback);
}

void Processor::onMid(function<void(float)> callback) FL_NO_EXCEPT {
    auto detector = getFrequencyBands();
    detector->onMidLevel.add(callback);
}

void Processor::onTreble(function<void(float)> callback) FL_NO_EXCEPT {
    auto detector = getFrequencyBands();
    detector->onTrebleLevel.add(callback);
}

void Processor::onFrequencyBands(function<void(float, float, float)> callback) FL_NO_EXCEPT {
    auto detector = getFrequencyBands();
    detector->onLevelsUpdate.add(callback);
}

void Processor::onEqualizer(function<void(const detector::Equalizer&)> callback) FL_NO_EXCEPT {
    auto detector = getEqualizerDetector();
    detector->onEqualizer.add(callback);
}

void Processor::onEnergy(function<void(float)> callback) FL_NO_EXCEPT {
    auto detector = getEnergyAnalyzer();
    detector->onEnergy.add(callback);
}

void Processor::onNormalizedEnergy(function<void(float)> callback) FL_NO_EXCEPT {
    auto detector = getEnergyAnalyzer();
    detector->onNormalizedEnergy.add(callback);
}

void Processor::onPeak(function<void(float)> callback) FL_NO_EXCEPT {
    auto detector = getEnergyAnalyzer();
    detector->onPeak.add(callback);
}

void Processor::onAverageEnergy(function<void(float)> callback) FL_NO_EXCEPT {
    auto detector = getEnergyAnalyzer();
    detector->onAverageEnergy.add(callback);
}

void Processor::onTransient(function<void()> callback) FL_NO_EXCEPT {
    auto detector = getTransientDetector();
    detector->onTransient.add(callback);
}

void Processor::onTransientWithStrength(function<void(float)> callback) FL_NO_EXCEPT {
    auto detector = getTransientDetector();
    detector->onTransientWithStrength.add(callback);
}

void Processor::onAttack(function<void(float)> callback) FL_NO_EXCEPT {
    auto detector = getTransientDetector();
    detector->onAttack.add(callback);
}

void Processor::onSilence(function<void(u8)> callback) FL_NO_EXCEPT {
    auto detector = getSilenceDetector();
    detector->onSilence.add(callback);
}

void Processor::onSilenceStart(function<void()> callback) FL_NO_EXCEPT {
    auto detector = getSilenceDetector();
    detector->onSilenceStart.add(callback);
}

void Processor::onSilenceEnd(function<void()> callback) FL_NO_EXCEPT {
    auto detector = getSilenceDetector();
    detector->onSilenceEnd.add(callback);
}

void Processor::onSilenceDuration(function<void(u32)> callback) FL_NO_EXCEPT {
    auto detector = getSilenceDetector();
    detector->onSilenceDuration.add(callback);
}

void Processor::onCrescendo(function<void()> callback) FL_NO_EXCEPT {
    auto detector = getDynamicsAnalyzer();
    detector->onCrescendo.add(callback);
}

void Processor::onDiminuendo(function<void()> callback) FL_NO_EXCEPT {
    auto detector = getDynamicsAnalyzer();
    detector->onDiminuendo.add(callback);
}

void Processor::onDynamicTrend(function<void(float)> callback) FL_NO_EXCEPT {
    auto detector = getDynamicsAnalyzer();
    detector->onDynamicTrend.add(callback);
}

void Processor::onCompressionRatio(function<void(float)> callback) FL_NO_EXCEPT {
    auto detector = getDynamicsAnalyzer();
    detector->onCompressionRatio.add(callback);
}

void Processor::onPitch(function<void(float)> callback) FL_NO_EXCEPT {
    auto detector = getPitchDetector();
    detector->onPitch.add(callback);
}

void Processor::onPitchWithConfidence(function<void(float, float)> callback) FL_NO_EXCEPT {
    auto detector = getPitchDetector();
    detector->onPitchWithConfidence.add(callback);
}

void Processor::onPitchChange(function<void(float)> callback) FL_NO_EXCEPT {
    auto detector = getPitchDetector();
    detector->onPitchChange.add(callback);
}

void Processor::onVoiced(function<void(u8)> callback) FL_NO_EXCEPT {
    auto detector = getPitchDetector();
    detector->onVoiced.add(callback);
}

void Processor::onNoteOn(function<void(u8, u8)> callback) FL_NO_EXCEPT {
    auto detector = getNoteDetector();
    detector->onNoteOn.add(callback);
}

void Processor::onNoteOff(function<void(u8)> callback) FL_NO_EXCEPT {
    auto detector = getNoteDetector();
    detector->onNoteOff.add(callback);
}

void Processor::onNoteChange(function<void(u8, u8)> callback) FL_NO_EXCEPT {
    auto detector = getNoteDetector();
    detector->onNoteChange.add(callback);
}

void Processor::onDownbeat(function<void()> callback) FL_NO_EXCEPT {
    auto detector = getDownbeatDetector();
    detector->onDownbeat.add(callback);
}

void Processor::onMeasureBeat(function<void(u8)> callback) FL_NO_EXCEPT {
    auto detector = getDownbeatDetector();
    detector->onMeasureBeat.add(callback);
}

void Processor::onMeterChange(function<void(u8)> callback) FL_NO_EXCEPT {
    auto detector = getDownbeatDetector();
    detector->onMeterChange.add(callback);
}

void Processor::onMeasurePhase(function<void(float)> callback) FL_NO_EXCEPT {
    auto detector = getDownbeatDetector();
    detector->onMeasurePhase.add(callback);
}

void Processor::onBackbeat(function<void(u8 beatNumber, float confidence, float strength)> callback) FL_NO_EXCEPT {
    auto detector = getBackbeatDetector();
    detector->onBackbeat.add(callback);
}

void Processor::onVocal(function<void(u8)> callback) FL_NO_EXCEPT {
    auto detector = getVocalDetector();
    detector->onVocal.add(callback);
}

void Processor::onVocalStart(function<void()> callback) FL_NO_EXCEPT {
    auto detector = getVocalDetector();
    detector->onVocalStart.add(callback);
}

void Processor::onVocalEnd(function<void()> callback) FL_NO_EXCEPT {
    auto detector = getVocalDetector();
    detector->onVocalEnd.add(callback);
}

void Processor::onVocalConfidence(function<void(float)> callback) FL_NO_EXCEPT {
    auto detector = getVocalDetector();
    // This callback fires every frame with the current confidence
    // We need to wrap it since detector::Vocal doesn't have this callback built-in
    detector->onVocal.add([callback, detector](u8) {
        if (callback) {
            callback(detector->getConfidence());
        }
    });
}

void Processor::onPercussion(function<void(detector::PercussionType)> callback) FL_NO_EXCEPT {
    auto detector = getPercussionDetector();
    detector->onPercussionHit.add(callback);
}

void Processor::onKick(function<void()> callback) FL_NO_EXCEPT {
    auto detector = getPercussionDetector();
    detector->onKick.add(callback);
}

void Processor::onSnare(function<void()> callback) FL_NO_EXCEPT {
    auto detector = getPercussionDetector();
    detector->onSnare.add(callback);
}

void Processor::onHiHat(function<void()> callback) FL_NO_EXCEPT {
    auto detector = getPercussionDetector();
    detector->onHiHat.add(callback);
}

void Processor::onTom(function<void()> callback) FL_NO_EXCEPT {
    auto detector = getPercussionDetector();
    detector->onTom.add(callback);
}

void Processor::onChord(function<void(const detector::Chord&)> callback) FL_NO_EXCEPT {
    auto detector = getChordDetector();
    detector->onChord.add(callback);
}

void Processor::onChordChange(function<void(const detector::Chord&)> callback) FL_NO_EXCEPT {
    auto detector = getChordDetector();
    detector->onChordChange.add(callback);
}

void Processor::onChordEnd(function<void()> callback) FL_NO_EXCEPT {
    auto detector = getChordDetector();
    detector->onChordEnd.add(callback);
}

void Processor::onKey(function<void(const detector::Key&)> callback) FL_NO_EXCEPT {
    auto detector = getKeyDetector();
    detector->onKey.add(callback);
}

void Processor::onKeyChange(function<void(const detector::Key&)> callback) FL_NO_EXCEPT {
    auto detector = getKeyDetector();
    detector->onKeyChange.add(callback);
}

void Processor::onKeyEnd(function<void()> callback) FL_NO_EXCEPT {
    auto detector = getKeyDetector();
    detector->onKeyEnd.add(callback);
}

void Processor::onMood(function<void(const detector::Mood&)> callback) FL_NO_EXCEPT {
    auto detector = getMoodAnalyzer();
    detector->onMood.add(callback);
}

void Processor::onMoodChange(function<void(const detector::Mood&)> callback) FL_NO_EXCEPT {
    auto detector = getMoodAnalyzer();
    detector->onMoodChange.add(callback);
}

void Processor::onValenceArousal(function<void(float, float)> callback) FL_NO_EXCEPT {
    auto detector = getMoodAnalyzer();
    detector->onValenceArousal.add(callback);
}

void Processor::onBuildupStart(function<void()> callback) FL_NO_EXCEPT {
    auto detector = getBuildupDetector();
    detector->onBuildupStart.add(callback);
}

void Processor::onBuildupProgress(function<void(float)> callback) FL_NO_EXCEPT {
    auto detector = getBuildupDetector();
    detector->onBuildupProgress.add(callback);
}

void Processor::onBuildupPeak(function<void()> callback) FL_NO_EXCEPT {
    auto detector = getBuildupDetector();
    detector->onBuildupPeak.add(callback);
}

void Processor::onBuildupEnd(function<void()> callback) FL_NO_EXCEPT {
    auto detector = getBuildupDetector();
    detector->onBuildupEnd.add(callback);
}

void Processor::onBuildup(function<void(const detector::Buildup&)> callback) FL_NO_EXCEPT {
    auto detector = getBuildupDetector();
    detector->onBuildup.add(callback);
}

void Processor::onDrop(function<void()> callback) FL_NO_EXCEPT {
    auto detector = getDropDetector();
    detector->onDrop.add(callback);
}

void Processor::onDropEvent(function<void(const detector::Drop&)> callback) FL_NO_EXCEPT {
    auto detector = getDropDetector();
    detector->onDropEvent.add(callback);
}

void Processor::onDropImpact(function<void(float)> callback) FL_NO_EXCEPT {
    auto detector = getDropDetector();
    detector->onDropImpact.add(callback);
}

void Processor::onVibeLevels(function<void(const detector::VibeLevels&)> callback) FL_NO_EXCEPT {
    auto detector = getVibeDetector();
    detector->onVibeLevels.add(callback);
}

void Processor::onVibeBassSpike(function<void()> callback) FL_NO_EXCEPT {
    auto detector = getVibeDetector();
    detector->onBassSpike.add(callback);
}

void Processor::onVibeMidSpike(function<void()> callback) FL_NO_EXCEPT {
    auto detector = getVibeDetector();
    detector->onMidSpike.add(callback);
}

void Processor::onVibeTrebSpike(function<void()> callback) FL_NO_EXCEPT {
    auto detector = getVibeDetector();
    detector->onTrebSpike.add(callback);
}

// ----- Polling Getter Implementations -----

// Helper: clamp float to 0..1
static float clamp01(float v) FL_NO_EXCEPT {
    if (v < 0.0f) return 0.0f;
    if (v > 1.0f) return 1.0f;
    return v;
}

// Helper: clamp float to -1..1
static float clampNeg1To1(float v) FL_NO_EXCEPT {
    if (v < -1.0f) return -1.0f;
    if (v > 1.0f) return 1.0f;
    return v;
}

float Processor::getVocalConfidence() FL_NO_EXCEPT {
    return clamp01(getVocalDetector()->getConfidence());
}

float Processor::getBeatConfidence() FL_NO_EXCEPT {
    return clamp01(getBeatDetector()->getConfidence());
}

float Processor::getBPM() FL_NO_EXCEPT {
    return getBeatDetector()->getBPM();
}

float Processor::getEnergy() FL_NO_EXCEPT {
    return clamp01(getEnergyAnalyzer()->getNormalizedRMS());
}

float Processor::getPeakLevel() FL_NO_EXCEPT {
    return clamp01(getEnergyAnalyzer()->getPeak());
}

float Processor::getBassLevel() FL_NO_EXCEPT {
    return clamp01(getFrequencyBands()->getBassNorm());
}

float Processor::getMidLevel() FL_NO_EXCEPT {
    return clamp01(getFrequencyBands()->getMidNorm());
}

float Processor::getTrebleLevel() FL_NO_EXCEPT {
    return clamp01(getFrequencyBands()->getTrebleNorm());
}

float Processor::getBassRaw() FL_NO_EXCEPT {
    return getFrequencyBands()->getBass();
}

float Processor::getMidRaw() FL_NO_EXCEPT {
    return getFrequencyBands()->getMid();
}

float Processor::getTrebleRaw() FL_NO_EXCEPT {
    return getFrequencyBands()->getTreble();
}

bool Processor::isSilent() FL_NO_EXCEPT {
    return getSilenceDetector()->isSilent();
}

u32 Processor::getSilenceDuration() FL_NO_EXCEPT {
    return getSilenceDetector()->getSilenceDuration();
}

float Processor::getTransientStrength() FL_NO_EXCEPT {
    return clamp01(getTransientDetector()->getStrength());
}

float Processor::getDynamicTrend() FL_NO_EXCEPT {
    return clampNeg1To1(getDynamicsAnalyzer()->getDynamicTrend());
}

bool Processor::isCrescendo() FL_NO_EXCEPT {
    return getDynamicsAnalyzer()->isCrescendo();
}

bool Processor::isDiminuendo() FL_NO_EXCEPT {
    return getDynamicsAnalyzer()->isDiminuendo();
}

float Processor::getPitchConfidence() FL_NO_EXCEPT {
    return clamp01(getPitchDetector()->getConfidence());
}

float Processor::getPitch() FL_NO_EXCEPT {
    return getPitchDetector()->getPitch();
}

float Processor::getTempoConfidence() FL_NO_EXCEPT {
    return clamp01(getTempoAnalyzer()->getConfidence());
}

float Processor::getTempoBPM() FL_NO_EXCEPT {
    return getTempoAnalyzer()->getBPM();
}

float Processor::getBuildupIntensity() FL_NO_EXCEPT {
    return clamp01(getBuildupDetector()->getIntensity());
}

float Processor::getBuildupProgress() FL_NO_EXCEPT {
    return clamp01(getBuildupDetector()->getProgress());
}

float Processor::getDropImpact() FL_NO_EXCEPT {
    return clamp01(getDropDetector()->getLastDrop().impact);
}

bool Processor::isKick() FL_NO_EXCEPT {
    return getPercussionDetector()->isKick();
}

bool Processor::isSnare() FL_NO_EXCEPT {
    return getPercussionDetector()->isSnare();
}

bool Processor::isHiHat() FL_NO_EXCEPT {
    return getPercussionDetector()->isHiHat();
}

bool Processor::isTom() FL_NO_EXCEPT {
    return getPercussionDetector()->isTom();
}

u8 Processor::getCurrentNote() FL_NO_EXCEPT {
    return getNoteDetector()->getCurrentNote();
}

float Processor::getNoteVelocity() FL_NO_EXCEPT {
    return getNoteDetector()->getLastVelocity() / 255.0f;
}

float Processor::getNoteConfidence() FL_NO_EXCEPT {
    return getNoteDetector()->isNoteActive() ? (getNoteDetector()->getLastVelocity() / 255.0f) : 0.0f;
}

float Processor::getDownbeatConfidence() FL_NO_EXCEPT {
    return clamp01(getDownbeatDetector()->getConfidence());
}

float Processor::getMeasurePhase() FL_NO_EXCEPT {
    return clamp01(getDownbeatDetector()->getMeasurePhase());
}

u8 Processor::getCurrentBeatNumber() FL_NO_EXCEPT {
    return getDownbeatDetector()->getCurrentBeat();
}

float Processor::getBackbeatConfidence() FL_NO_EXCEPT {
    return clamp01(getBackbeatDetector()->getConfidence());
}

float Processor::getBackbeatStrength() FL_NO_EXCEPT {
    return clamp01(getBackbeatDetector()->getStrength());
}

float Processor::getChordConfidence() FL_NO_EXCEPT {
    return clamp01(getChordDetector()->getCurrentChord().confidence);
}

float Processor::getKeyConfidence() FL_NO_EXCEPT {
    return clamp01(getKeyDetector()->getCurrentKey().confidence);
}

float Processor::getMoodArousal() FL_NO_EXCEPT {
    return clamp01(getMoodAnalyzer()->getArousal());
}

float Processor::getMoodValence() FL_NO_EXCEPT {
    return clampNeg1To1(getMoodAnalyzer()->getValence());
}

float Processor::getVibeBass() FL_NO_EXCEPT {
    return getVibeDetector()->getBass();
}

float Processor::getVibeMid() FL_NO_EXCEPT {
    return getVibeDetector()->getMid();
}

float Processor::getVibeTreb() FL_NO_EXCEPT {
    return getVibeDetector()->getTreb();
}

float Processor::getVibeVol() FL_NO_EXCEPT {
    return getVibeDetector()->getVol();
}

float Processor::getVibeBassAtt() FL_NO_EXCEPT {
    return getVibeDetector()->getBassAtt();
}

float Processor::getVibeMidAtt() FL_NO_EXCEPT {
    return getVibeDetector()->getMidAtt();
}

float Processor::getVibeTrebAtt() FL_NO_EXCEPT {
    return getVibeDetector()->getTrebAtt();
}

float Processor::getVibeVolAtt() FL_NO_EXCEPT {
    return getVibeDetector()->getVolAtt();
}

bool Processor::isVibeBassSpike() FL_NO_EXCEPT {
    return getVibeDetector()->isBassSpike();
}

bool Processor::isVibeMidSpike() FL_NO_EXCEPT {
    return getVibeDetector()->isMidSpike();
}

bool Processor::isVibeTrebSpike() FL_NO_EXCEPT {
    return getVibeDetector()->isTrebSpike();
}

float Processor::getEqBass() FL_NO_EXCEPT {
    return clamp01(getEqualizerDetector()->getBass());
}

float Processor::getEqMid() FL_NO_EXCEPT {
    return clamp01(getEqualizerDetector()->getMid());
}

float Processor::getEqTreble() FL_NO_EXCEPT {
    return clamp01(getEqualizerDetector()->getTreble());
}

float Processor::getEqVolume() FL_NO_EXCEPT {
    return clamp01(getEqualizerDetector()->getVolume());
}

float Processor::getEqVolumeNormFactor() FL_NO_EXCEPT {
    return clamp01(getEqualizerDetector()->getVolumeNormFactor());
}

float Processor::getEqZcf() FL_NO_EXCEPT {
    return clamp01(getEqualizerDetector()->getZcf());
}

float Processor::getEqBin(int index) FL_NO_EXCEPT {
    return clamp01(getEqualizerDetector()->getBin(index));
}

float Processor::getEqAutoGain() FL_NO_EXCEPT {
    return getEqualizerDetector()->getAutoGain();
}

bool Processor::getEqIsSilence() FL_NO_EXCEPT {
    return getEqualizerDetector()->getIsSilence();
}

float Processor::getEqDominantFreqHz() FL_NO_EXCEPT {
    return getEqualizerDetector()->getDominantFreqHz();
}

float Processor::getEqDominantMagnitude() FL_NO_EXCEPT {
    return clamp01(getEqualizerDetector()->getDominantMagnitude());
}

float Processor::getEqVolumeDb() FL_NO_EXCEPT {
    return getEqualizerDetector()->getVolumeDb();
}

void Processor::setSampleRate(int sampleRate) FL_NO_EXCEPT {
    mSampleRate = sampleRate;
    mContext->setSampleRate(sampleRate);

    // Propagate to all active detector that are sample-rate-aware
    for (auto& d : mActiveDetectors) {
        d->setSampleRate(sampleRate);
    }
}

int Processor::getSampleRate() const FL_NO_EXCEPT {
    return mSampleRate;
}

void Processor::setGain(float gain) FL_NO_EXCEPT {
    mGain = gain;
}

float Processor::getGain() const FL_NO_EXCEPT {
    return mGain;
}

void Processor::setSignalConditioningEnabled(bool enabled) FL_NO_EXCEPT {
    mSignalConditioningEnabled = enabled;
}

void Processor::setNoiseFloorTrackingEnabled(bool enabled) FL_NO_EXCEPT {
    mNoiseFloorTrackingEnabled = enabled;
}

void Processor::configureSignalConditioner(const SignalConditionerConfig& config) FL_NO_EXCEPT {
    mSignalConditioner.configure(config);
    mSignalConditioningEnabled = true;
}

void Processor::configureNoiseFloorTracker(const NoiseFloorTrackerConfig& config) FL_NO_EXCEPT {
    mNoiseFloorTracker.configure(config);
    mNoiseFloorTrackingEnabled = config.enabled;
}

void Processor::setMicProfile(MicProfile profile) FL_NO_EXCEPT {
    mMicProfile = profile;
    if (mEqualizerDetector) {
        mEqualizerDetector->setMicProfile(profile);
    }
}

void Processor::configureEqualizer(const detector::EqualizerConfig& config) FL_NO_EXCEPT {
    getEqualizerDetector()->configure(config);
}

const Sample& Processor::getSample() const FL_NO_EXCEPT {
    return mContext->getSample();
}

void Processor::reset() FL_NO_EXCEPT {
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

shared_ptr<detector::Beat> Processor::getBeatDetector() FL_NO_EXCEPT {
    if (!mBeatDetector) {
        mBeatDetector = make_shared<detector::Beat>();
        registerDetector(mBeatDetector);
    }
    return mBeatDetector;
}

shared_ptr<detector::FrequencyBands> Processor::getFrequencyBands() FL_NO_EXCEPT {
    if (!mFrequencyBands) {
        mFrequencyBands = make_shared<detector::FrequencyBands>();
        registerDetector(mFrequencyBands);
    }
    return mFrequencyBands;
}

shared_ptr<detector::EnergyAnalyzer> Processor::getEnergyAnalyzer() FL_NO_EXCEPT {
    if (!mEnergyAnalyzer) {
        mEnergyAnalyzer = make_shared<detector::EnergyAnalyzer>();
        registerDetector(mEnergyAnalyzer);
    }
    return mEnergyAnalyzer;
}

shared_ptr<detector::TempoAnalyzer> Processor::getTempoAnalyzer() FL_NO_EXCEPT {
    if (!mTempoAnalyzer) {
        mTempoAnalyzer = make_shared<detector::TempoAnalyzer>();
        registerDetector(mTempoAnalyzer);
    }
    return mTempoAnalyzer;
}

shared_ptr<detector::Transient> Processor::getTransientDetector() FL_NO_EXCEPT {
    if (!mTransientDetector) {
        mTransientDetector = make_shared<detector::Transient>();
        registerDetector(mTransientDetector);
    }
    return mTransientDetector;
}

shared_ptr<detector::Silence> Processor::getSilenceDetector() FL_NO_EXCEPT {
    if (!mSilenceDetector) {
        mSilenceDetector = make_shared<detector::Silence>();
        registerDetector(mSilenceDetector);
    }
    return mSilenceDetector;
}

shared_ptr<detector::DynamicsAnalyzer> Processor::getDynamicsAnalyzer() FL_NO_EXCEPT {
    if (!mDynamicsAnalyzer) {
        mDynamicsAnalyzer = make_shared<detector::DynamicsAnalyzer>();
        registerDetector(mDynamicsAnalyzer);
    }
    return mDynamicsAnalyzer;
}

shared_ptr<detector::Pitch> Processor::getPitchDetector() FL_NO_EXCEPT {
    if (!mPitchDetector) {
        mPitchDetector = make_shared<detector::Pitch>();
        registerDetector(mPitchDetector);
    }
    return mPitchDetector;
}

shared_ptr<detector::Note> Processor::getNoteDetector() FL_NO_EXCEPT {
    if (!mNoteDetector) {
        // Share the detector::Pitch instance between Processor and detector::Note
        auto pitchDetector = getPitchDetector();
        mNoteDetector = make_shared<detector::Note>(pitchDetector);
        registerDetector(mNoteDetector);
    }
    return mNoteDetector;
}

shared_ptr<detector::Downbeat> Processor::getDownbeatDetector() FL_NO_EXCEPT {
    if (!mDownbeatDetector) {
        // Share the detector::Beat instance between Processor and detector::Downbeat
        auto beatDetector = getBeatDetector();
        mDownbeatDetector = make_shared<detector::Downbeat>(beatDetector);
        registerDetector(mDownbeatDetector);
    }
    return mDownbeatDetector;
}

shared_ptr<detector::Backbeat> Processor::getBackbeatDetector() FL_NO_EXCEPT {
    if (!mBackbeatDetector) {
        // Share the detector::Beat and detector::Downbeat instances with detector::Backbeat
        auto beatDetector = getBeatDetector();
        auto downbeatDetector = getDownbeatDetector();
        mBackbeatDetector = make_shared<detector::Backbeat>(beatDetector, downbeatDetector);
        registerDetector(mBackbeatDetector);
    }
    return mBackbeatDetector;
}

shared_ptr<detector::Vocal> Processor::getVocalDetector() FL_NO_EXCEPT {
    if (!mVocalDetector) {
        mVocalDetector = make_shared<detector::Vocal>();
        registerDetector(mVocalDetector);
    }
    return mVocalDetector;
}

shared_ptr<detector::Percussion> Processor::getPercussionDetector() FL_NO_EXCEPT {
    if (!mPercussionDetector) {
        mPercussionDetector = make_shared<detector::Percussion>();
        registerDetector(mPercussionDetector);
    }
    return mPercussionDetector;
}

shared_ptr<detector::ChordDetector> Processor::getChordDetector() FL_NO_EXCEPT {
    if (!mChordDetector) {
        mChordDetector = make_shared<detector::ChordDetector>();
        registerDetector(mChordDetector);
    }
    return mChordDetector;
}

shared_ptr<detector::KeyDetector> Processor::getKeyDetector() FL_NO_EXCEPT {
    if (!mKeyDetector) {
        mKeyDetector = make_shared<detector::KeyDetector>();
        registerDetector(mKeyDetector);
    }
    return mKeyDetector;
}

shared_ptr<detector::MoodAnalyzer> Processor::getMoodAnalyzer() FL_NO_EXCEPT {
    if (!mMoodAnalyzer) {
        mMoodAnalyzer = make_shared<detector::MoodAnalyzer>();
        registerDetector(mMoodAnalyzer);
    }
    return mMoodAnalyzer;
}

shared_ptr<detector::BuildupDetector> Processor::getBuildupDetector() FL_NO_EXCEPT {
    if (!mBuildupDetector) {
        mBuildupDetector = make_shared<detector::BuildupDetector>();
        registerDetector(mBuildupDetector);
    }
    return mBuildupDetector;
}

shared_ptr<detector::DropDetector> Processor::getDropDetector() FL_NO_EXCEPT {
    if (!mDropDetector) {
        mDropDetector = make_shared<detector::DropDetector>();
        registerDetector(mDropDetector);
    }
    return mDropDetector;
}

shared_ptr<detector::EqualizerDetector> Processor::getEqualizerDetector() FL_NO_EXCEPT {
    if (!mEqualizerDetector) {
        mEqualizerDetector = make_shared<detector::EqualizerDetector>();
        if (mMicProfile != MicProfile::None) {
            mEqualizerDetector->setMicProfile(mMicProfile);
        }
        registerDetector(mEqualizerDetector);
    }
    return mEqualizerDetector;
}

shared_ptr<detector::Vibe> Processor::getVibeDetector() FL_NO_EXCEPT {
    if (!mVibeDetector) {
        mVibeDetector = make_shared<detector::Vibe>();
        registerDetector(mVibeDetector);
    }
    return mVibeDetector;
}

shared_ptr<Processor> Processor::createWithAutoInput(
        shared_ptr<IInput> input) FL_NO_EXCEPT {
    auto processor = make_shared<Processor>();
    processor->mAudioInput = input;
    // weak_ptr so the lambda doesn't prevent Processor destruction
    weak_ptr<Processor> weak = processor;
    weak_ptr<IInput> weakInput = input;
    processor->mAutoTask = fl::task::every_ms(1).then([weak, weakInput]() {
        auto proc = weak.lock();
        auto inp = weakInput.lock();
        if (!proc || !inp) {
            return;
        }
        vector_inlined<Sample, 16> samples;
        inp->readAll(&samples);
        for (const auto& sample : samples) {
            proc->update(sample);
        }
    });
    return processor;
}

} // namespace audio
} // namespace fl
