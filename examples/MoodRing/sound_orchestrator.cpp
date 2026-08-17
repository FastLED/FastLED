// sound_orchestrator.cpp - implementation of the 3-state audio orchestrator.
#include "sound_orchestrator.h"

#include "fl/math/math.h"
#include "fl/stl/chrono.h"

namespace mood_ring {

namespace {

// State -> visual bank lookup tables.
// Per issue #2713 / #2256 mapping:
//   Silence      -> calm ambient (SLOW_FADE, WATER, PARAMETRIC_WATER, FLUFFY_BLOBS)
//   Disorganized -> energy/spectrum (RGB_BLOBS5, WAVES, POLAR_WAVES, COMPLEX_KALEIDO)
//   BpmLocked    -> beat geometry (RINGS, CHASING_SPIRALS, SPIRALUS, CENTER_FIELD)
constexpr fl::AnimartrixAnim kSilenceBank[] = {
    fl::AnimartrixAnim::SLOW_FADE,
    fl::AnimartrixAnim::WATER,
    fl::AnimartrixAnim::PARAMETRIC_WATER,
    fl::AnimartrixAnim::FLUFFY_BLOBS,
};
constexpr fl::AnimartrixAnim kDisorganizedBank[] = {
    fl::AnimartrixAnim::RGB_BLOBS5,
    fl::AnimartrixAnim::WAVES,
    fl::AnimartrixAnim::POLAR_WAVES,
    fl::AnimartrixAnim::COMPLEX_KALEIDO,
};
constexpr fl::AnimartrixAnim kBpmLockedBank[] = {
    fl::AnimartrixAnim::RINGS,
    fl::AnimartrixAnim::CHASING_SPIRALS,
    fl::AnimartrixAnim::SPIRALUS,
    fl::AnimartrixAnim::CENTER_FIELD,
};

// Cycle a bank slowly so a long-held state still has visual variety.
constexpr fl::u32 kAnimCyclePeriodMs = 18000;

template <typename Arr>
fl::AnimartrixAnim pickFromBank(const Arr &bank, fl::u32 nowMs) {
    const fl::u32 n = sizeof(bank) / sizeof(bank[0]);
    const fl::u32 idx = (nowMs / kAnimCyclePeriodMs) % n;
    return bank[idx];
}

} // namespace

SoundOrchestrator::SoundOrchestrator(
    fl::shared_ptr<fl::audio::Processor> processor,
    fl::shared_ptr<fl::Animartrix> animartrix,
    fl::FxEngine *engine)
    : mProcessor(processor), mAnimartrix(animartrix), mEngine(engine) {}

void SoundOrchestrator::begin() {
    if (!mProcessor) return;

    // Capture event-style audio cues for BpmLocked state.
    // The Processor stores a single callback per event; we install closures
    // that mark "last seen at" timestamps which the per-frame tick consults.
    // (We can't capture `this->mLastKickMs` directly by reference in a
    // function<void()> stored by Processor across callback re-registration
    // without UAF risk, so we route through `this`.)
    SoundOrchestrator *self = this;
    mProcessor->onKick([self]() {
        self->mLastKickMs = fl::millis();
    });
    mProcessor->onSnare([self]() {
        self->mLastSnareMs = fl::millis();
    });
    mProcessor->onDownbeat([self]() {
        self->mLastDownbeatMs = fl::millis();
        self->mDownbeatCount++;
    });
}

fl::AnimartrixAnim SoundOrchestrator::pickAnimationFor(SoundState s, fl::u32 nowMs) {
    switch (s) {
    case SoundState::Silence:      return pickFromBank(kSilenceBank, nowMs);
    case SoundState::Disorganized: return pickFromBank(kDisorganizedBank, nowMs);
    case SoundState::BpmLocked:    return pickFromBank(kBpmLockedBank, nowMs);
    }
    return fl::AnimartrixAnim::SLOW_FADE;
}

void SoundOrchestrator::switchAnimationIfNeeded(SoundState /*newState*/, fl::u32 nowMs) {
    fl::AnimartrixAnim desired = pickAnimationFor(mState, nowMs);
    if (desired != mCurrentAnim && mAnimartrix) {
        mCurrentAnim = desired;
        mAnimartrix->fxSet(static_cast<int>(desired));
    }
}

SoundState SoundOrchestrator::classify(fl::u32 nowMs) {
    if (!mProcessor) return SoundState::Silence;

    const bool isSilent = mProcessor->isSilent();

    // Silence has its own asymmetric hysteresis (enter slow, exit fast) so a
    // single audio sample can wake us up promptly but a brief gap won't drop
    // us into ambient mode mid-song.
    if (isSilent) {
        if (mSilentSinceMs == 0) mSilentSinceMs = nowMs;
        mNonSilentSinceMs = 0;
    } else {
        if (mNonSilentSinceMs == 0) mNonSilentSinceMs = nowMs;
        mSilentSinceMs = 0;
    }

    // --- raw signals ---
    const float tempoConf = mProcessor->getTempoConfidence();
    const float beatConf  = mProcessor->getBeatConfidence();

    // Silence-exit gate: when we're currently in Silence, require the
    // configured silenceExitMs of *contiguous* non-silent audio before we
    // even consider leaving. Without this, mNonSilentSinceMs would be
    // tracked but never consulted, so a single non-silent frame would
    // immediately bounce us out -- defeating the asymmetric silence
    // hysteresis documented on OrchestratorConfig.
    const bool silenceReleased =
        !isSilent &&
        mNonSilentSinceMs != 0 &&
        (nowMs - mNonSilentSinceMs) >= mCfg.silenceExitMs;

    // --- determine the candidate (instantaneous) state ---
    SoundState instant;
    if (isSilent && mSilentSinceMs && (nowMs - mSilentSinceMs) >= mCfg.silenceEnterMs) {
        instant = SoundState::Silence;
    } else if (mState == SoundState::Silence && !silenceReleased) {
        // Hold Silence until the non-silent run reaches silenceExitMs.
        instant = SoundState::Silence;
    } else if (!isSilent &&
               tempoConf >= mCfg.tempoConfidenceEnter &&
               beatConf  >= mCfg.beatConfidenceEnter) {
        instant = SoundState::BpmLocked;
    } else if (mState == SoundState::BpmLocked &&
               (tempoConf >= mCfg.tempoConfidenceExit &&
                beatConf  >= mCfg.beatConfidenceExit)) {
        // Stay locked: we're below "enter" but above "exit" thresholds.
        instant = SoundState::BpmLocked;
    } else if (mState == SoundState::Silence && isSilent) {
        instant = SoundState::Silence;
    } else {
        instant = SoundState::Disorganized;
    }

    // --- hysteresis: candidate must hold for classifierHysteresisMs AND
    //     current state must have served minDwellMs before we accept the switch.
    if (instant != mState) {
        if (instant != mCandidate) {
            mCandidate = instant;
            mCandidateSinceMs = nowMs;
        }
        const fl::u32 candidateHeld = nowMs - mCandidateSinceMs;
        const fl::u32 stateHeld     = nowMs - mStateEnteredAtMs;
        if (candidateHeld >= mCfg.classifierHysteresisMs &&
            stateHeld     >= mCfg.minDwellMs) {
            return instant;
        }
        return mState; // not yet -- hold current state
    }

    // Candidate matches current state; reset candidate tracker.
    mCandidate = mState;
    mCandidateSinceMs = nowMs;
    return mState;
}

float SoundOrchestrator::tick(fl::u32 nowMs, float manualSpeedScalar) {
    if (mStateEnteredAtMs == 0) mStateEnteredAtMs = nowMs;

    const SoundState newState = classify(nowMs);
    if (newState != mState) {
        mState = newState;
        mStateEnteredAtMs = nowMs;
    }
    switchAnimationIfNeeded(mState, nowMs);

    // Derive the bus, then apply only the field the engine owns. Everything
    // else on the bus is for the overlay and, later, the second engine.
    if (mProcessor) {
        AudioEvents events;
        events.lastKickMs = mLastKickMs;
        events.lastSnareMs = mLastSnareMs;
        events.lastDownbeatMs = mLastDownbeatMs;
        events.hiHat = mProcessor->isHiHat();
        events.beatNumber = mProcessor->getCurrentBeatNumber();
        mDeriver.derive(*mProcessor, mState, mBusCfg, events, nowMs,
                        manualSpeedScalar, &mBus);
    } else {
        mBus = VisualControlBus{};
        mBus.transportSpeed = manualSpeedScalar;
    }

    if (mEngine) mEngine->setSpeed(mBus.transportSpeed);
    mLastEngineSpeed = mBus.transportSpeed;
    return mBus.transportSpeed;
}

} // namespace mood_ring
