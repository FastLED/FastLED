# Audio Silence Gate — Design

**Status:** Design approved, Phase 1 in progress
**Tracks:** [FastLED #2253](https://github.com/FastLED/FastLED/issues/2253) — volume not returning to zero in silence
**Scope:** `src/fl/audio/` — `Vibe`, `Reactive` (dominant freq / magnitude / flux), `TempoAnalyzer`

## Problem

FastLED's audio-reactive metrics do not return to zero when audio input stops:

- `Vibe::getVol()` converges to **1.0** during silence (MilkDrop self-normalization — `mImm / mLongAvg → noise / noise → 1.0`, and the `< 0.001f` clamp in `vibe.cpp.hpp:86-88` hard-codes `mImmRel = 1.0f`).
- `Reactive::Data::volume` decays but takes ~2 s because `EnergyAnalyzer::mRunningMaxFilter` has a 2 s decay tau and a hard floor of 1.0 on the denominator.
- `Reactive::Data::dominantFrequency` / `magnitude` / `spectralFlux` are instantaneous FFT reads with **no decay** — they lock onto whatever FFT bin is strongest (usually numerical noise) during silence.
- `TempoAnalyzer::mCurrentBPM` is updated only on onset detection → **persists unchanged** through silence.

Root cause: the pipeline already computes a high-quality silence signal (`NoiseFloorTracker::isAboveFloor()` at `noise_floor_tracker.h:95`) but its output is never consumed by any detector. The tracker is orphaned.

## Approach — two cooperating layers

### Layer 1: Silence flag in Context (plumbing)

One boolean field on `audio::Context` populated by `Processor`/`Reactive` from `NoiseFloorTracker::isAboveFloor()`. This gives every detector a zero-cost, frame-wide signal-quality signal without refactoring detector interfaces.

```cpp
// audio_context.h — new methods
void setSilent(bool silent) FL_NOEXCEPT { mIsSilent = silent; }
bool isSilent() const FL_NOEXCEPT { return mIsSilent; }

// audio_processor.cpp.hpp — after NoiseFloorTracker.update()
if (mNoiseFloorTrackingEnabled && conditioned.isValid()) {
    mNoiseFloorTracker.update(conditioned.rms());
    mContext->setSilent(!mNoiseFloorTracker.isAboveFloor(conditioned.rms()));
} else {
    mContext->setSilent(false);  // tracking disabled → never claim silence
}
```

Default behavior preserved: `mNoiseFloorTrackingEnabled` stays `false` by default, so `isSilent()` returns `false` for existing users who haven't opted in. Silence gating is a user opt-in via `Processor::enableNoiseFloorTracker(true)` or the equivalent `ReactiveConfig`.

### Layer 2: `SilenceEnvelope` — reusable decay helper

A small composable class that detectors embed to translate the boolean flag into smooth per-metric decay. This preserves detector algorithm integrity (MilkDrop math stays MilkDrop math during audio) and gives each detector a tuned decay rate.

```cpp
class SilenceEnvelope {
public:
    struct Config {
        float decayTauSeconds = 0.5f;   // Time constant for decay-to-zero
        float targetValue = 0.0f;       // What to decay toward (usually 0)
    };

    SilenceEnvelope() FL_NOEXCEPT;
    explicit SilenceEnvelope(const Config& cfg) FL_NOEXCEPT;
    void configure(const Config& cfg) FL_NOEXCEPT;

    // Pass-through during audio; exponential decay to targetValue during silence.
    float update(bool isSilent, float currentValue, float dt) FL_NOEXCEPT;

    // Snap to a fresh value (called on re-attack or reset)
    void reset(float initialValue = 0.0f) FL_NOEXCEPT;

    // True once the envelope has decayed to within epsilon of target
    bool isGated(float epsilon = 1e-4f) const FL_NOEXCEPT;
};
```

Semantics:
- `!isSilent` → return `currentValue` unchanged, cache it as the last live value
- `isSilent && |lastLive| > epsilon` → exponentially decay the cached value toward `targetValue` with rate `exp(-dt / decayTauSeconds)`
- On silence→audio transition, snap back to the fresh `currentValue` (no attack lag — users want beats to hit hard)

## Phase-by-phase plan

### Phase 1 — foundations (parallelizable)

| ID  | Scope | Files | LOC | Blocks | Blocked by |
|-----|-------|-------|-----|--------|-----------|
| PR-A | Silence flag on Context + Processor/Reactive populate it | `audio_context.h/.cpp.hpp`, `audio_processor.cpp.hpp`, `audio_reactive.cpp.hpp`, new test | ~60 | C, D, E | — |
| PR-B | `SilenceEnvelope` class + unit tests | `silence_envelope.h/.cpp.hpp`, new test, `_build.cpp.hpp` | ~120 | C, D, E | — |

**PR-A and PR-B touch disjoint files — can ship in parallel (same branch, different commits, or independent branches).**

### Phase 2 — adoption (parallelizable, after Phase 1)

| ID  | Scope | Files | LOC | Blocked by |
|-----|-------|-------|-----|-----------|
| PR-C | Vibe adopts silence gate (fixes user-visible `getVol()` → 1.0 bug) | `vibe.h`, `vibe.cpp.hpp`, test | ~60 | A, B |
| PR-D | Reactive adopts: `dominantFrequency`, `magnitude`, `spectralFlux` gate | `audio_reactive.cpp.hpp`, test | ~40 | A, B |
| PR-E | TempoAnalyzer adopts BPM fast-decay (tau ~2 s) | `tempo_analyzer.cpp.hpp`, test | ~50 | A, B |

**PR-C/D/E touch disjoint detector files — can ship in parallel once A + B land.**

## API additions (full detail)

### `audio::Context`
```cpp
void setSilent(bool silent) FL_NOEXCEPT;
bool isSilent() const FL_NOEXCEPT;
```
- Default state is `false` (no silence claim).
- Cleared to `false` on `setSample()` (per-frame reset — Processor/Reactive must re-populate after NFT update).

### `audio::SilenceEnvelope` (new)
See Layer 2 above. Lives in `src/fl/audio/silence_envelope.{h,cpp.hpp}`. Added to `src/fl/audio/_build.cpp.hpp` unity include. Unit test at `tests/fl/audio/silence_envelope.cpp`.

### No breaking changes
- All existing getters unchanged.
- Default behavior unchanged for users who haven't enabled `NoiseFloorTracker`.
- New behavior is strictly additive — silence gate kicks in only when (a) NFT enabled AND (b) detector adopts `SilenceEnvelope`.

## Testing strategy

### Per-layer unit tests

- **Context silence flag**: feed a sample, assert `isSilent()` returns what `setSilent()` set; assert `setSample()` resets to false.
- **SilenceEnvelope**:
  - Pass-through during audio (flag=false, returns currentValue)
  - Exponential decay during silence to tau within 5% at `3 * decayTauSeconds`
  - Snap-back on silence→audio transition (no attack lag)
  - `isGated()` returns true once decayed below epsilon

### Integration tests (Phase 2)

- **Vibe silence test**: drive Vibe with 1 s of 440 Hz sine, then 3 s of true silence. Assert `getVol()` crosses below 0.01 within 1 s of silence start.
- **Reactive spectral silence test**: same drive pattern. Assert `dominantFrequency == 0.0f` and `magnitude == 0.0f` after 1 s silence.
- **BPM decay test**: drive TempoAnalyzer with a 120 BPM click track for 5 s, then silence. Assert BPM confidence → 0 within 3 s.

### Regression tests

- All existing audio tests must still pass unchanged (no breaking API).
- `deficiencies.cpp` (the TDD-style test file for known issues) gets three new entries that were previously failing and now pass.

## Risks & open questions

- **Default NFT off**: if users don't enable NFT, silence gate does nothing. Should we flip NFT on by default after this lands? *Decision: defer to a separate PR with benchmarks showing no perf regression. Silence gate is opt-in for v1.*
- **SilenceEnvelope decay tau per detector**: Vibe wants ~0.5 s (fast, matches visual expectation). BPM wants ~2 s (musical tempo lingers naturally). Spectral wants ~0.2 s (FFT noise floor is brittle). These are tunable constants; initial values chosen by ear, may need tuning after user feedback.
- **Interaction with signal conditioner noise gate**: the conditioner already zeros PCM below RMS 300. If the conditioner gates aggressively, detectors see zero PCM → RMS drops → NFT sees silence → flag goes high. Consistent. No conflict.
- **Reactive's own NFT instance**: Reactive has its own `mNoiseFloorTracker` separate from `Processor`'s. Both paths need to populate the Context silence flag. Done in both `Processor::update()` and `Reactive::processSample()`.

## Out of scope

- Unified volume concept (`AudioLevel` struct replacing all volume getters) — considered and rejected as too churn-heavy; preserved as Option 2 in the discussion. Can be layered on top later if demand emerges.
- Subscriber / event model on `NoiseFloorTracker` (Option 5) — Silence detector already fires transition events; no need to duplicate.
- Signal conditioner changes.

## Revision history

- 2026-04-18: Initial draft (Claude, for zackees)
