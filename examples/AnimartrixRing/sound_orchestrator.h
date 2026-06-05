// sound_orchestrator.h - 3-state audio orchestrator for AnimartrixRing.
//
// Classifies the live audio stream into one of three states:
//   * Silence            -> ambient visuals (slow, restrained)
//   * Disorganized       -> energy / spectrum visuals (bass radial, mid hue,
//                           treble sparkle); time warp is allowed but secondary
//   * BpmLocked          -> beat geometry (rings/spirals) driven by kick,
//                           snare, downbeat, measure phase
//
// Hysteresis: each state has a minimum dwell time so the classifier does not
// chatter on borderline audio. Transition out of a state requires both:
//   1. the entry condition for a *different* state to be satisfied for at
//      least kClassifierHysteresisMs of contiguous audio, and
//   2. the current state to have been held for at least kMinDwellMs.
//
// This file is intentionally sketch-scoped: the building blocks it consumes
// (Processor::isSilent / getTempoConfidence / getBeatConfidence / getEqBass /
// onDownbeat / onKick / onSnare / getMeasurePhase / getVibeBass etc.) all
// already live in src/fl/audio/. We're orchestrating, not adding primitives.
#pragma once

#include "FastLED.h"
#include "fl/audio/audio_processor.h"
#include "fl/audio/detector/vibe.h"
#include "fl/fx/2d/animartrix.hpp"
#include "fl/fx/fx_engine.h"
#include "fl/stl/shared_ptr.h"
#include "fl/stl/stdint.h"

namespace animartrix_ring {

enum class SoundState : fl::u8 {
    Silence = 0,
    Disorganized = 1,
    BpmLocked = 2,
};

const char *toString(SoundState s);

struct OrchestratorConfig {
    // Classifier thresholds.
    float tempoConfidenceEnter = 0.60f;  ///< enter BpmLocked when tempoConf >= this
    float tempoConfidenceExit  = 0.45f;  ///< leave  BpmLocked when tempoConf <  this
    float beatConfidenceEnter  = 0.50f;  ///< also require beatConf >= this for BpmLocked
    float beatConfidenceExit   = 0.35f;
    fl::u32 silenceEnterMs     = 700;    ///< silence must persist this long to enter Silence
    fl::u32 silenceExitMs      = 150;    ///< sound must persist this long to leave Silence

    // Minimum dwell time per state (anti-chatter floor).
    fl::u32 minDwellMs         = 1500;

    // How long the *other* state's entry condition must hold continuously
    // before we accept a transition (additional hysteresis on top of dwell).
    fl::u32 classifierHysteresisMs = 400;

    // BpmLocked: pulse decay (ms) for a single kick/snare/downbeat event.
    fl::u32 pulseDecayMs       = 220;

    // Disorganized: how much vibe.bass scales engine speed (above baseline 1.0).
    float disorganizedSpeedSpan = 1.5f;

    // Silence: ambient engine speed.
    float silenceSpeed         = 0.25f;

    // BpmLocked: engine baseline speed (BPM modulation rides on top).
    float bpmLockedBaseSpeed   = 1.0f;
};

/// Top-level orchestrator. Owns no audio data of its own; polls the supplied
/// Processor on every tick() and drives the supplied FxEngine / Animartrix.
class SoundOrchestrator {
public:
    SoundOrchestrator(fl::shared_ptr<fl::audio::Processor> processor,
                      fl::shared_ptr<fl::Animartrix> animartrix,
                      fl::FxEngine *engine);

    /// Wire up audio callbacks (downbeat/kick/snare). Call once after the
    /// Processor is created. Safe to call multiple times -- last call wins
    /// because the Processor stores a single callback per event.
    void begin();

    /// Per-frame tick. Pass the current millis() and a manual-speed scalar
    /// (so the existing "Time Speed" slider still composes).
    /// Returns the engine speed actually applied this tick.
    float tick(fl::u32 nowMs, float manualSpeedScalar);

    /// Observability.
    SoundState state() const { return mState; }
    float lastEngineSpeed() const { return mLastEngineSpeed; }
    fl::u32 stateEnteredAtMs() const { return mStateEnteredAtMs; }

    /// Override config (e.g. from UI sliders). Cheap; safe to call every tick.
    void setConfig(const OrchestratorConfig &cfg) { mCfg = cfg; }
    const OrchestratorConfig &config() const { return mCfg; }

private:
    // Visual bank selection per state.
    static fl::AnimartrixAnim pickAnimationFor(SoundState s, fl::u32 nowMs);
    void switchAnimationIfNeeded(SoundState newState, fl::u32 nowMs);

    // Per-state behavior.
    float driveSilence(fl::u32 nowMs, float manualSpeedScalar);
    float driveDisorganized(fl::u32 nowMs, float manualSpeedScalar);
    float driveBpmLocked(fl::u32 nowMs, float manualSpeedScalar);

    // Classifier.
    SoundState classify(fl::u32 nowMs);

private:
    fl::shared_ptr<fl::audio::Processor> mProcessor;
    fl::shared_ptr<fl::Animartrix> mAnimartrix;
    fl::FxEngine *mEngine;

    OrchestratorConfig mCfg{};

    SoundState mState = SoundState::Silence;
    fl::u32    mStateEnteredAtMs = 0;
    fl::AnimartrixAnim mCurrentAnim = fl::AnimartrixAnim::SLOW_FADE;

    // Classifier hysteresis: how long a *candidate* state has held continuously.
    SoundState mCandidate = SoundState::Silence;
    fl::u32    mCandidateSinceMs = 0;

    // Silence hysteresis: separate timers because Silence enters slow / leaves fast.
    fl::u32    mSilentSinceMs = 0;   // 0 = "not currently silent"
    fl::u32    mNonSilentSinceMs = 0;

    // BpmLocked pulse state. Updated by event callbacks.
    fl::u32 mLastKickMs = 0;
    fl::u32 mLastSnareMs = 0;
    fl::u32 mLastDownbeatMs = 0;
    fl::u8  mDownbeatCount = 0;

    float mLastEngineSpeed = 1.0f;
};

} // namespace animartrix_ring
