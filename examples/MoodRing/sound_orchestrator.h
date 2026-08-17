// sound_orchestrator.h - 3-state audio orchestrator for MoodRing.
//
// Originally landed in examples/AnimartrixRing (issue #2713, PR #2809) and
// moved here when AnimartrixRing was reduced to a pure sampling demo. This is
// the sound layer of the MoodRing product sketch -- see issue #2256.
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

#include "visual_control_bus.h"

namespace mood_ring {

// SoundState and toString() live in visual_control_bus.h -- they are shared
// vocabulary between the classifier, the bus derivation, and the overlay.

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

};

// NOTE: speed/pulse tuning used to live here. It moved to BusConfig and the
// per-state policy in visual_control_bus.cpp when the bus landed (#3885):
// events now drive a visible pulse on the ring instead of yanking the clock.

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
    /// (so the existing "Time Speed" slider still composes). Classifies the
    /// audio, switches the animation bank, derives the visual control bus, and
    /// applies bus.transportSpeed to the engine.
    /// Returns the engine speed actually applied this tick.
    float tick(fl::u32 nowMs, float manualSpeedScalar);

    /// The bus derived on the most recent tick. Consumers read this rather
    /// than the raw detectors.
    const VisualControlBus &bus() const { return mBus; }

    /// Bus tunables (base speed, punch gain). Cheap; safe to set every tick.
    void setBusConfig(const BusConfig &cfg) { mBusCfg = cfg; }
    const BusConfig &busConfig() const { return mBusCfg; }

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

    // Classifier.
    SoundState classify(fl::u32 nowMs);

private:
    fl::shared_ptr<fl::audio::Processor> mProcessor;
    fl::shared_ptr<fl::Animartrix> mAnimartrix;
    fl::FxEngine *mEngine;

    OrchestratorConfig mCfg{};
    BusConfig mBusCfg{};
    BusDeriver mDeriver;
    VisualControlBus mBus{};

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

} // namespace mood_ring
