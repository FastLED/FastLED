# FastLED Audio→MIDI: Spectral-Peak Polyphony Upgrade (Design Doc)

## 0) Summary

Augment FastLED’s current polyphonic path with a Spectrotune-style **spectral peak** pipeline that remains MCU-friendly. We do **not** vendor Spectrotune; we adapt the ideas:

* Windowed FFT + **parabolic peak interpolation**
* Lightweight **spectral tilt (linear EQ)**
* Configurable **noise smoothing**
* **Octave masks** (enable/disable octave bands)
* Toggleable **harmonic filter / fundamental veto**
* Optional **PCP (12-bin chroma) stabilizer**
* Runtime **threshold** control
* Optional **octave→MIDI channel mapping**

All features are behind compile-time and runtime flags so the default remains lean.

---

## 1) Goals & Non-Goals

**Goals**

* Improve **polyphonic accuracy** (dense chords, quiet notes).
* Improve **robustness** (noise, rumble, hiss, spectral leakage).
* Keep **real-time** on ESP32-class MCUs (S3/C3/C6/P4) at typical sample rates (16–48 kHz) with FFT sizes 512–2048.
* Minimal heap churn; deterministic latency.

**Non-Goals**

* No heavyweight ML models.
* No file/codec I/O (still upstream PCM).
* No GUI; we expose hooks/params only.

---

## 2) Constraints (MCU)

* **CPU**: ~3–7 ms/frame budget at 16 kHz, 1024-pt FFT (ESP32-S3 @240–240+ MHz).
* **RAM**: < 20–40 KB incremental (FFT buffers, temp arrays, PCP history).
* **Latency**: 20–80 ms typical (FRAME_SIZE/SR + processing), must be stable.
* **Portability**: Works with ARDUINO/PlatformIO; no dynamic allocations in tight loops.

---

## 3) Architecture Overview

```
[PCM frames] → [Window] → [FFT] → [Mag Spectrum] → [Tilt EQ] → [Smoothing]
                                                       ↓
                                        [Peak Find + Parabolic Interp]
                                                       ↓
                      [Harmonic Filter] ←→ [Octave Mask] ←→ [PCP Stabilizer (optional)]
                                                       ↓
                                        [Note Tracker + Onset/Offset]
                                                       ↓
                                         [MIDI Events / Callbacks]
```

---

## 4) Components & Algorithms

### 4.1 Windowing (configurable)

* **Add**: `Hann` (default), `Hamming`, `Blackman` (optional).
* Rationale: reduces leakage → clearer peaks.
* **Implementation**: precompute window tables per FFT size in PROGMEM/flash; apply in-place multiply before FFT.

### 4.2 FFT

* Continue using existing FFT backend (or KissFFT / ESP-DSP depending on target).
* Support N = 512/1024/2048 (power-of-two).
* Output: magnitude `M[k] = sqrt(Re^2+Im^2)` or `|Re| + |Im|` (cheaper approximation) gated by config.

### 4.3 Spectral Tilt (linear EQ)

* **Add**: one-parameter tilt (slope in dB/decade), applied as:

  * `M'[k] = M[k] * (k / k_ref)^α`, with α computed from slope.
  * Fixed-point: precompute per-bin gain Q15.
* Rationale: compensate pinkish spectra; help high-freq notes.

### 4.4 Noise Smoothing (selectable)

* **Modes**: `NONE`, `BOX3`, `TRI5` (triangular), `ADJAVG` (2-sided adjacent avg).
* Apply to `M'` prior to peak picking.
* Rationale: suppress narrowband noise; stabilize peaks.
* Impl: 1D FIR across bins (constant-time per bin).

### 4.5 Peak Picking + Parabolic Interpolation

* Find local maxima `k0` where `M'[k0]` is above **threshold** `T`.
* **Parabolic Interp** (3-point):

  * Given `y_-1 = M'[k0-1], y0 = M'[k0], y_+1 = M'[k0+1]`,
  * fractional offset: `δ = 0.5 * (y_-1 - y_+1) / (y_-1 - 2*y0 + y_+1)`, clamp |δ|≤0.5
  * refined bin: `k* = k0 + δ`
  * frequency: `f = k* * (SR / N)`
* Rationale: reduce binning error; improve separation of close pitches.

### 4.6 Octave Masks (runtime)

* **Add**: bitmask over octaves [0..7] (or computed from MIDI note range).
* Computation:

  * Convert candidate `f` → MIDI `n = round(69 + 12*log2(f/440))`.
  * Determine `oct = floor(n/12)`; accept only if `oct_mask & (1<<oct)`.
* Rationale: ignore rumble/hiss bands quickly.

### 4.7 Harmonic Filter / Fundamental Veto (toggle)

* **Motivation**: avoid double-counting overtones as fundamentals.
* Strategy:

  1. Sort peaks by magnitude descending.
  2. For each accepted fundamental `f0`, **veto** peaks near integer multiples `m*f0` (m=2..4) within cents tolerance (e.g., ±35 cents), **unless** the “harmonic > fundamental” rule triggers (configurable).
  3. Optional back-projection: test if putative `f0/2` exists at expected energy ratio; if yes, demote current to harmonic.
* Config:

  * `harmonic_filter_enable` (bool)
  * `harmonic_tolerance_cents` (default 35)
  * `harmonic_energy_ratio_max` (e.g., overtone must be < α·fundamental; α default 0.7)

### 4.8 PCP (Chroma) Stabilizer (optional, low-cost)

* **Add**: 12-bin chroma accumulator over last `K` frames (e.g., K=8–16).
* For each candidate note `n`, compute pitch-class `pc = n mod 12`.
* Maintain EMA per `pc`: `C[pc] ← (1-β)·C[pc] + β·1` when pc appears; decay others.
* **Use**: when two candidates are near threshold, **bias** acceptance toward higher `C[pc]`.
* Rationale: simple musical context → fewer octave flips / spurious notes.
* Memory: 12 floats (or Q15), negligible.

### 4.9 Thresholding (runtime-tunable)

* Global magnitude threshold `T` (in linear or dB), adjustable at runtime via API.
* Optionally **adaptive**: `T = μ + λ·σ` computed from noise floor estimate in non-musical bins.
* Keep default static `T` for deterministic behavior; expose setter for advanced users.

### 4.10 Note Tracking (onset/offset)

* Reuse current monophonic tracker logic per note id, extended for multiple notes:

  * **Onset**: require `N_on` consecutive frames above pitch & magnitude confidence.
  * **Offset**: require `N_off` consecutive frames below.
  * **Hysteresis**: semitone stickiness to avoid chatter.
* Velocity = scaled RMS or **per-peak magnitude** mapped to 0..127.

### 4.11 Optional: Octave→MIDI Channel Mapping

* If enabled, map note’s **octave** to MIDI channel (e.g., C1..B1 → ch1, …, C6..B6 → ch6).
* Zero-cost if disabled.

---

## 5) Public API (proposed)

```cpp
namespace fl { namespace audio {

enum class Window : uint8_t { Hann, Hamming, Blackman };
enum class Smooth : uint8_t { None, Box3, Tri5, AdjAvg };

struct PolySpecCfg {
  // FFT / window
  uint16_t fft_size = 1024;         // 512/1024/2048
  Window   window   = Window::Hann;

  // Spectral conditioning
  float spectral_tilt_db_per_dec = 0.0f; // e.g., +3.0 boosts highs
  Smooth smoothing = Smooth::Box3;

  // Detection
  float peak_threshold_db = -40.0f; // runtime-adjustable
  bool  harmonic_filter = true;
  float harmonic_tol_cents = 35.f;
  float harmonic_energy_ratio_max = 0.7f;

  // Octave mask: bit i enables octave i (MIDI 12*i..12*i+11)
  uint8_t octave_mask = 0xFF;       // enable all by default

  // PCP stabilizer
  bool  pcp_enable = false;
  uint8_t pcp_history = 12;         // frames (EMA depth)
  float pcp_bias = 0.1f;            // weight in acceptance

  // Note gating
  uint8_t onset_frames = 2;         // frames required for NoteOn
  uint8_t offset_frames = 2;        // frames required for NoteOff

  // Velocity
  bool velocity_from_peak = true;   // else from RMS

  // Mapping
  bool octave_to_midi_channel = false;
};

class PolySpec {
public:
  explicit PolySpec(const PolySpecCfg& cfg);

  // Process one frame of PCM (mono float16/float32/int16)
  void process_frame(const float* pcm, int n_samples);

  // Runtime controls
  void set_peak_threshold_db(float db);
  void set_octave_mask(uint8_t mask);
  void set_spectral_tilt(float db_per_dec);
  void set_smoothing(Smooth s);

  // Callback registration
  using NoteOnFn  = void(*)(uint8_t midi_note, uint8_t velocity, uint8_t channel, void* user);
  using NoteOffFn = void(*)(uint8_t midi_note, uint8_t channel, void* user);
  void on_note_on(NoteOnFn f, void* user);
  void on_note_off(NoteOffFn f, void* user);

private:
  // opaque
};

}} // namespace
```

* Backward-compatible: existing `sound_to_midi` remains; `PolySpec` is a new path under `fx/audio/`.
* Allow float or int16 input; internal convert to float or Q15 depending on build flag.

---

## 6) Performance & Memory Budget

| Feature                      | CPU impact              | RAM impact           | Notes                    |
| ---------------------------- | ----------------------- | -------------------- | ------------------------ |
| Window multiply              | O(N)                    | N floats (table)     | Precompute per FFT size  |
| FFT (1024)                   | O(N log N)              | 2N (complex buffers) | Existing                 |
| Magnitude (approx)           | O(N/2)                  | none                 | `abs(Re)+abs(Im)` option |
| Tilt EQ                      | O(N/2)                  | N/2 gains (Q15)      | Precompute               |
| Smoothing (Box3/Tri5)        | O(N/2)                  | none                 | FIR 1D                   |
| Peak pick + parabolic interp | O(P) with P≪N           | none                 | ~5–20 peaks              |
| Harmonic filter              | O(P^2) worst-case small | none                 | P limited (e.g., ≤16)    |
| PCP stabilizer               | O(P) + 12 state         | 12 scalars           | Optional                 |

**Target**: ≤ 4 ms per 1024-pt frame on ESP32-S3 @ 240 MHz (typical).

---

## 7) Fixed-Point & Fast Paths (build flags)

* `FASTLED_AUDIO_FIXEDPOINT`: use Q15 magnitude approximation, Q15 tilt gains, int16 PCM.
* `FASTLED_AUDIO_MAG_ABS1`: use `abs(Re)+abs(Im)` for magnitude (no sqrt).
* `FASTLED_AUDIO_NO_TILT`: compile out tilt stage.
* `FASTLED_AUDIO_NO_PCP`: compile out PCP code.
* `FASTLED_AUDIO_SMALL_FFT`: limit to 512/1024 tables.

These let downstream users trim CPU/RAM.

---

## 8) Tracking & MIDI Emission

* **De-dup within frame**: after harmonic filtering, cap max new notes per frame (e.g., 8).
* **Semitone stickiness**: for existing notes, ignore small freq drift (±40 cents) to prevent re-note.
* **Velocity**: map peak magnitude via log scale to 0..127; clamp. If `velocity_from_peak=false`, use RMS gate at onset.
* **Channel**: if `octave_to_midi_channel`, compute `channel = base + octave`.

---

## 9) Testing Plan

### Unit tests

* Parabolic interpolation correctness (synthetic sinusoids at fractional bins).
* Harmonic veto logic (fundamental vs 2×/3× peaks).
* Octave mask acceptance/rejection.
* PCP biasing (preference under ambiguous two-note collision).

### Golden audio sets

* **Single tones** (A4 sweeps; low/high SNR).
* **Intervals & chords** (3rds, 4ths, 5ths, clustered tritones).
* **Noisy beds** (pink/white noise underlay; rumble @ 60–120 Hz).
* **Percussive onsets** (piano/guitar strums).
* **Edge cases**: detuned pairs (±20–40 cents), close partials.

Metrics:

* Precision/recall on note events, onset timing error (ms), false positive rate in silence.

### Benchmarks

* Measure per-frame execution on ESP32-S3/C3/C6/P4 at 16 kHz/1024 FFT.
* Memory audit (static + peak).

---

## 10) Migration & Backward Compatibility

* Existing `sound_to_midi` API unchanged.
* New `PolySpec` class opt-in; default config mirrors current behavior as closely as possible (Hann, Box3, modest threshold, harmonic on, PCP off).
* Compile flags default to **minimal features** unless enabled.

---

## 11) Rollout (Phased)

1. **Phase A** (core accuracy):

   * Windowing, parabolic interpolation, smoothing, threshold runtime setter.
2. **Phase B** (robustness):

   * Harmonic filter toggle with ratios, octave mask.
3. **Phase C** (musical context & UX):

   * PCP stabilizer, octave→channel mapping, adaptive threshold (optional).

Each phase gated by unit tests + embedded benchmarks.

---

## 12) Risks & Mitigations

* **CPU spikes** (many peaks): cap P (e.g., keep top-K by magnitude).
* **Mis-veto fundamentals**: expose harmonic toggle; log debug counters.
* **PCP bias wrong key**: keep PCP weight small (pcp_bias ≤ 0.15) and decay quickly; easy off switch.
* **Config complexity**: provide sane defaults; document 2–3 recommended profiles (“Low-CPU”, “Balanced”, “Hi-Accuracy”).

---

## 13) Developer Notes (Implementation Tips)

* Precompute `bin→midi` lookup for fast octave mapping.
* Keep all arrays static/stack; avoid malloc in `process_frame`.
* Use `constexpr` tables for windows/tilt where possible.
* Consider `esp_dsp` FFT on ESP32 for speed; fall back to portable FFT.
* Provide a debug hook to dump per-frame peaks (freq, mag, accepted/vetoed) for profiling.

---

## 14) Example (minimal usage)

```cpp
#include <fastled/fx/audio/poly_spec.h>

using namespace fl::audio;

static void note_on(uint8_t n, uint8_t v, uint8_t ch, void*)  { /* send MIDI */ }
static void note_off(uint8_t n, uint8_t ch, void*)            { /* send MIDI */ }

PolySpecCfg cfg;
cfg.fft_size = 1024;
cfg.window = Window::Hann;
cfg.spectral_tilt_db_per_dec = +3.0f;
cfg.smoothing = Smooth::Box3;
cfg.peak_threshold_db = -42.0f;
cfg.harmonic_filter = true;
cfg.octave_mask = 0b01111110; // octaves 1..6
cfg.pcp_enable = false;

PolySpec poly(cfg);
poly.on_note_on(note_on, nullptr);
poly.on_note_off(note_off, nullptr);

// In audio callback:
void audio_frame(const float* pcm, int n) {
  poly.process_frame(pcm, n); // n == FRAME_SIZE
}
```

---

## 15) Documentation (to add)

* Tuning guide: threshold, tilt, smoothing presets.
* “When to enable PCP” cheat sheet.
* Known limitations (very dense, detuned cluster chords; very low SR + tiny FFT).
* Performance table per MCU.

---

### Done Right, This Gives Us:

* Better chord coverage (clearer peaks via window+interp).
* Fewer octave/harmonic mistakes (octave mask + harmonic veto).
* More stable behavior across material (tilt + smoothing + optional PCP).
* Still **real-time** and MCU-friendly, with compile-time levers to stay lean.
