# CURRENT TASK: Fix BeatDetection WASM Compilation

## Problem
The `examples/BeatDetection/BeatDetection.ino` example fails to compile to WASM because the cached libfastled library doesn't include the latest `beat_detector.h` changes (ParticleFilter support).

**Compilation Errors:**
```
error: no member named 'ParticleFilter' in 'fl::TempoTrackerType'
error: no member named 'pf_num_particles' in 'fl::BeatDetectorConfig'
error: no member named 'pf_tempo_std_dev' in 'fl::BeatDetectorConfig'
error: no member named 'pf_phase_std_dev' in 'fl::BeatDetectorConfig'
error: no member named 'pf_resample_threshold' in 'fl::BeatDetectorConfig'
```

**Root Cause:**
The WASM compilation server uses a cached precompiled header and library (`/build/quick/libfastled.a`, `/build/quick/fastled_pch.h`) that was built before ParticleFilter was added to `beat_detector.h`.

The server output shows:
- "Skipping library recompilation - only asset files changed"
- The library was compiled with old headers

**Evidence:**
- `src/fx/audio/beat_detector.h` line 85: `ParticleFilter` enum value EXISTS
- `src/fx/audio/beat_detector.h` lines 131-135: All `pf_*` config fields EXIST
- `examples/BeatDetection/BeatDetection.ino` lines 117, 124-127: Code correctly uses these features
- WASM compilation uses cached library that predates these changes

## Required Solution
Force rebuild the WASM libfastled library to include the latest beat_detector.h changes. Options to investigate:

1. **Stop and restart the local WASM compilation server** to force cache invalidation
2. **Find the Docker container** running the server and restart it
3. **Purge the Docker build cache** for the WASM compilation environment
4. **Force library recompilation** through fled options (--force-compile didn't work, need different approach)

## Success Criteria
```bash
fled examples/BeatDetection --just-compile
```
Should compile successfully with ParticleFilter features.

## Notes
- The example compiles successfully when changed to use `CombFilter` instead (verified)
- This confirms the issue is only the cached library, not the source code
- The local server was at http://localhost:9021 but failed to start properly
- Fell back to https://fastled.onrender.com which also has old cache

---

# BACKGROUND: FastLED Task Specification: Polymetric Beat Detection & Particle Visualization

## Executive Summary

Extend the existing **BeatDetector** system with polymetric rhythm analysis (N/M overlay support) and create a particle-based LED visualization engine. Generic algorithm with profile presets for different musical styles (Tipper, Aphex Twin, etc.).

**Status:** COMPLETE - All phases implemented, tested, and refactored ✅

## Phase 3 Summary + Refactoring

Phase 3 has been successfully completed with the following achievements:

1. **PolymetricBeats Fx Class**: Created high-level Fx2d effect class that integrates all components:
   - Inherits from Fx2d following FastLED patterns
   - Owns BeatDetector and RhythmParticles instances
   - **Generic algorithm** supporting arbitrary N/M overlays
   - Provides simple, user-friendly API

2. **Complete Callback Wiring**: Automatic routing of all events:
   - Beat detector onset callbacks → Particle emitters
   - Polymetric beat events → Particle system
   - Subdivision events → Particle micro-timing
   - Fill detection → Overlay particle bursts

3. **Audio Processing Integration**:
   - `processAudio()` method feeds frames to BeatDetector
   - Automatic FFT and onset detection
   - Real-time particle emission based on rhythmic events

4. **Rendering Pipeline**:
   - `draw()` method handles animation loop
   - Configurable background fade for trailing effects
   - Optional clear-on-beat for impact effects
   - Automatic particle physics updates and LED rendering

5. **Configuration Management**:
   - Unified PolymetricBeatsConfig structure
   - Runtime reconfiguration support
   - Access to all underlying components
   - **Profile system**: `PolymetricProfiles::Tipper()` for preset configurations

6. **Comprehensive Testing**: 11 integration tests validating end-to-end functionality

**Refactoring (Post-Phase 3):**
- Renamed `TipperBeats` → `PolymetricBeats` (generic algorithm name)
- Extracted Tipper-specific config as `PolymetricProfiles::Tipper()` preset
- Emphasized generic polymetric capabilities in documentation
- All components now clearly separated: Algorithm (generic) vs Profile (style-specific)

**Completed Files:**
- `src/fx/2d/polymetric_beats.h` - PolymetricBeats Fx class with generic API + profile presets
- `src/fx/2d/polymetric_beats.cpp` - Integration implementation with callback wiring
- `tests/test_polymetric_beats.cpp` - 11 integration tests

**Status**: All three phases complete + refactored! Ready for Phase 4 (Example & Documentation).

📝 **Cleanup Guide**: See **`CLEAN.md`** for comprehensive documentation of:
- All 10 new files and 2 modified files
- Technical debt items with exact locations
- Performance optimization opportunities (prioritized)
- Refactoring suggestions
- API stability guarantees
- Breaking changes to avoid

## Phase 2 Summary

Phase 2 has been successfully completed with the following achievements:

1. **Structure-of-Arrays (SoA) Particle Pool**: Implemented cache-efficient particle storage with separate arrays for position, velocity, color (HSV), and lifetime data

2. **Multi-Emitter System**: Created four specialized emitters responding to different rhythmic events:
   - Kick emitter (bass onsets) - orange/red particles with high velocity
   - Snare emitter (mid onsets) - cyan particles with medium velocity
   - Hi-hat emitter (high onsets) - yellow particles with low velocity
   - Overlay emitter (polymetric/fill events) - purple particles

3. **Physics Engine**: Implemented complete particle dynamics:
   - Radial gravity (attraction/repulsion from center)
   - Curl noise flow field using FastLED's inoise16_raw for smooth particle motion
   - Velocity decay for natural damping
   - Boundary wrapping for continuous motion

4. **Kick Ducking Effect**: Added dynamic brightness reduction on kick hits to simulate sidechaining (configurable amount and duration)

5. **LED Rendering**: Complete rendering pipeline with:
   - 2D→1D LED mapping
   - HSV→RGB conversion with hue variance per emitter
   - Additive blending for particle accumulation
   - Optional bloom effect for bright particles

6. **Lifecycle Management**: Automatic particle birth/death with fade-out based on remaining lifetime

7. **Comprehensive Testing**: 15 unit tests covering initialization, emission, physics, rendering, and configuration

**Next Steps**: Phase 3 will create the high-level Fx class that integrates BeatDetector and RhythmParticles.

## Phase 1 Summary

Phase 1 has been successfully completed with the following achievements:

1. **Per-band Onset Detection**: Extended BeatDetector to expose bass, mid, and high-frequency band onsets separately via dedicated callbacks (`onOnsetBass`, `onOnsetMid`, `onOnsetHigh`)

2. **Polymetric Analysis**: Created new `PolymetricAnalyzer` class that tracks:
   - 4/4 base meter phase (0.0-1.0)
   - 7/8 overlay phase that drifts against the base meter
   - 16th note subdivisions
   - Swing/humanize micro-timing offsets
   - Fill detection based on polymetric tension

3. **Callbacks Integration**: Added new callbacks to BeatDetector:
   - `onPolymetricBeat(phase4_4, phase7_8)` - Fires on each beat with both phase values
   - `onSubdivision(type, swing_offset)` - Fires on subdivision boundaries (16ths, triplets, etc.)
   - `onFill(starting, density)` - Fires when fill sections are detected

4. **Phase Accessors**: Added public methods to query current phases:
   - `getPhase4_4()` - Current 4/4 bar phase
   - `getPhase7_8()` - Current overlay cycle phase
   - `getPhase16th()` - Current 16th note phase

5. **Testing**: Comprehensive unit tests validate all polymetric functionality

**Next Steps**: Phase 2 will implement the particle system that responds to these rhythmic events.

## Background & Context

### Related Systems (VALIDATED)

- ✅ **Existing:** `src/fx/audio/beat_detector.{h,cpp}` - SuperFlux onset detection + tempo tracking
  - Already implements: SuperFlux, multi-band onset detection, adaptive whitening, comb filter tempo tracking
  - Already has callbacks: `onOnset(confidence, timestamp_ms)`, `onBeat(confidence, bpm, timestamp_ms)`

- ✅ **Existing:** `src/fx/audio/sound_to_midi.{h,cpp}` - Multi-band spectral analysis

- ✅ **Existing:** `examples/BeatDetection/BeatDetection.ino` - Current beat visualization with UISlider controls

- ✅ **Existing UI System:** `src/fl/ui.h` - UISlider, UINumberField, UICheckbox, UIGroup, UIAudio

### Musical Context

Tipper's production style features:
- 4/4 foundation with polymetric 7/8 overlay patterns
- Micro-timed swing (triplets, quintuplets)
- Multi-band onset complexity (kick, snare, hi-hats operate independently)
- "Broken 4/4" with syncopated fills and humanized timing

## Requirements

### Functional Requirements

**FR1: Polymetric Beat Analysis (NEW - extends BeatDetector)**
- Detect 4/4 base meter (existing tempo tracker can handle this ✅)
- **NEW:** Add 7/8 overlay phase tracker (7 pulses per 2 bars)
- **NEW:** Micro-timing engine: swing quantization (±12-25% of subdivision)
- **NEW:** Tuplet detection: triplet/quintuplet fill patterns

**FR2: Multi-Band Onset Detection (PARTIALLY EXISTS)**
- ✅ Existing: SuperFlux with multi-band onset detection already implemented
- ✅ Existing: Adaptive thresholds with debouncing (minimum inter-onset intervals)
- **NEW:** Expose per-band onset callbacks (bass/mid/high) - currently combined into single onset callback

**FR3: Particle System (NEW)**
- SoA (structure-of-arrays) layout for cache efficiency
- Emitter system tied to rhythmic events:
  - Kick emitter (bass onsets + quarter notes)
  - Snare/glitch emitter (mid onsets + syncopated 16ths)
  - Hi-hat spray emitter (high onsets + triplet subdivisions)
  - Overlay spiral emitter (7/8 phase accents)
- Physics: radial gravity, curl noise field, kick ducking
- Follow FastLED namespace conventions (`fl::`)

**FR4: LED Rendering (NEW)**
- Map particles to LED strips/matrices
- HSV color system with audio-reactive palettes
- Optional bloom effect (1-tap exponential blur)

### Non-Functional Requirements

**NFR1: Performance (ESP32-S3 @ 240MHz)**
- Audio processing: ≤50% of one core @ 44.1kHz
- Particle update + render: ≥60 FPS with 1000 particles
- Memory: ≤24KB additional RAM (configurable via macros)

**NFR2: Real-Time Constraints**
- Zero heap allocations in audio/render loops
- All buffers statically allocated or arena-based
- Frame-to-frame jitter <5ms (95th percentile)

**NFR3: Code Quality**
- Follow FastLED namespace conventions (`fl::`)
- Use FastLED types (`CRGB`, `fl::vector`, `fl::function`)
- Include comprehensive unit tests
- Document all public APIs with Doxygen

## Architecture

### Module Structure (UPDATED)

```
src/fx/audio/
  ├── beat_detector.{h,cpp}             [MODIFY - add polymetric callbacks]
  └── polymetric_analyzer.{h,cpp}       [NEW - 7/8 overlay tracker]

src/fx/particles/
  └── rhythm_particles.{h,cpp}          [NEW - particle system]

examples/Fx/
  └── RhythmicParticles/RhythmicParticles.ino [NEW - demo with UISlider controls]
```

### Data Flow

```
PCM Audio (44.1kHz)
  ↓
BeatDetector (existing - SuperFlux + tempo tracker) ✅
  ├→ Base tempo (4/4) + beat phase
  └→ Multi-band onsets (already computed internally, need to expose via callbacks)
      ↓
PolymetricAnalyzer [NEW]
  ├→ 7/8 overlay phase
  ├→ Swing/tuplet timing
  └→ Fill detection
      ↓
Event Bus (callbacks - extend existing BeatDetector callbacks)
  ├→ onBeat(quarter, phase4_4, phase7_8)          [EXTEND existing onBeat]
  ├→ onSubdivision(16th, swingOffset)              [NEW callback]
  ├→ onOnsetBass/Mid/High(confidence, time)        [NEW - split existing onOnset]
  └→ onFill(start/end, density)                    [NEW callback]
      ↓
RhythmParticles [NEW]
  ├→ Emitter management
  ├→ Physics simulation (SoA)
  └→ Particle lifecycle
      ↓
LED Renderer
  └→ CRGB* output (strip/matrix)
```

### Public APIs

#### Extension to BeatDetectorConfig (beat_detector.h) - MODIFY EXISTING

```cpp
struct BeatDetectorConfig {
    // ... existing fields (sample_rate_hz, frame_size, hop_size, etc.) ...

    // NEW: Polymetric analysis
    bool enable_polymetric = false;       ///< Enable 7/8 overlay analysis
    int overlay_numerator = 7;            ///< Overlay meter numerator (7 for 7/8)
    int overlay_bars = 2;                 ///< Overlay cycle length (bars)
    float swing_amount = 0.12f;           ///< Swing 0.0-0.25 (0=straight, 0.25=hard swing)
    float humanize_ms = 4.0f;             ///< Micro-timing jitter ±ms
    bool enable_tuplet_detection = true;  ///< Detect triplet/quintuplet fills
};
```

#### New Callbacks for BeatDetector - EXTEND EXISTING CLASS

```cpp
class BeatDetector {
public:
    // EXISTING callbacks (keep as-is) ✅
    fl::function<void(float confidence, float timestamp_ms)> onOnset;
    fl::function<void(float confidence, float bpm, float timestamp_ms)> onBeat;

    // NEW: Polymetric callbacks (add to existing class)
    fl::function<void(float phase4_4, float phase7_8)> onPolymetricBeat;
    fl::function<void(int subdivision, float swingOffset)> onSubdivision;
    fl::function<void(float confidence, float timestamp_ms)> onOnsetBass;
    fl::function<void(float confidence, float timestamp_ms)> onOnsetMid;
    fl::function<void(float confidence, float timestamp_ms)> onOnsetHigh;
    fl::function<void(bool starting, float density)> onFill;

    // NEW: Phase accessors
    float getPhase4_4() const;
    float getPhase7_8() const;
    float getPhase16th() const;
};
```

#### New: RhythmParticles class (src/fx/particles/rhythm_particles.h)

```cpp
namespace fl {

struct RhythmParticlesConfig {
    int max_particles = 1000;            ///< Maximum particle count
    float dt = 1.0f / 120.0f;            ///< Simulation timestep (120 FPS default)
    float velocity_decay = 0.985f;       ///< Velocity damping per frame
    float radial_gravity = 0.0f;         ///< Radial pull to center
    float curl_strength = 0.7f;          ///< Flow field intensity
    float kick_duck_amount = 0.35f;      ///< Brightness duck on kick (0-1)
    float kick_duck_duration_ms = 80.0f; ///< Duck duration
    uint8_t bloom_threshold = 64;        ///< Bloom activation threshold (0-255)
    float bloom_strength = 0.5f;         ///< Bloom intensity
    int width = 32;                      ///< Logical canvas width
    int height = 8;                      ///< Logical canvas height
};

struct ParticleEmitterConfig {
    float emit_rate = 10.0f;             ///< Particles per event
    float velocity_min = 0.5f;           ///< Min initial velocity
    float velocity_max = 2.0f;           ///< Max initial velocity
    float life_min = 0.5f;               ///< Min lifetime (seconds)
    float life_max = 2.0f;               ///< Max lifetime (seconds)
    CRGB color_base = CRGB::White;       ///< Base color
    uint8_t hue_variance = 30;           ///< Hue randomization ±
};

class RhythmParticles {
public:
    explicit RhythmParticles(const RhythmParticlesConfig& cfg);

    // Connect to beat detector events
    void onBeat(float phase4_4, float phase7_8);
    void onSubdivision(int subdiv, float swingOffset);
    void onOnsetBass(float confidence, float timestamp_ms);
    void onOnsetMid(float confidence, float timestamp_ms);
    void onOnsetHigh(float confidence, float timestamp_ms);
    void onFill(bool starting, float density);

    // Simulation
    void update(float dt);               ///< Advance physics
    void render(CRGB* leds, int num_leds); ///< Render to LED buffer

    // Configuration
    void setEmitterConfig(const char* emitter_name, const ParticleEmitterConfig& cfg);
    void setPalette(const CRGBPalette16& palette);

    // Stats
    int getActiveParticleCount() const;

private:
    // SoA particle storage
    struct ParticlePool {
        static constexpr int MAX = 2048;
        float x[MAX], y[MAX], z[MAX];
        float vx[MAX], vy[MAX], vz[MAX];
        uint8_t h[MAX], s[MAX], v[MAX];
        float life[MAX];
        int count = 0;
    } _pool;

    void emitParticles(const char* emitter, int count);
    void applyForces();
    void updateLifetime(float dt);
    void cullDead();
};

} // namespace fl
```

## Implementation Plan

### Phase 1: Extend BeatDetector (Week 1) ✅ COMPLETE
- [x] Add per-band onset callbacks (bass/mid/high) by exposing existing multi-band analysis
- [x] Implement 7/8 overlay phase tracker in new `PolymetricAnalyzer` class
- [x] Add swing/humanize micro-timing engine
- [x] Create new callback system (extend existing `BeatDetector`)
- [x] Unit tests for polymetric analysis

**Completed Files:**
- `src/fx/audio/beat_detector.h` - Added per-band onset callbacks (onOnsetBass/Mid/High), polymetric callbacks (onPolymetricBeat, onSubdivision, onFill), phase accessors
- `src/fx/audio/beat_detector.cpp` - Integrated PolymetricAnalyzer, wired callbacks, added per-band peak picking
- `src/fx/audio/polymetric_analyzer.h` - New class for 7/8 overlay tracking, swing/humanize micro-timing
- `src/fx/audio/polymetric_analyzer.cpp` - Implementation of polymetric rhythm analysis
- `tests/test_polymetric_beat.cpp` - Comprehensive unit tests for polymetric analysis

### Phase 2: Particle System Core (Week 2) ✅ COMPLETE
- [x] Implement SoA particle pool in new `RhythmParticles` class
- [x] Create emitter system (kick, snare, hat, overlay)
- [x] Physics: gravity, curl noise, ducking field
- [x] Particle lifecycle management
- [x] Unit tests for particle dynamics

**Completed Files:**
- `src/fx/particles/rhythm_particles.h` - RhythmParticles class with SoA layout, emitter configs, physics settings
- `src/fx/particles/rhythm_particles.cpp` - Complete implementation: emission, physics (gravity/curl noise/ducking), lifecycle, rendering with bloom
- `tests/test_rhythm_particles.cpp` - 15 comprehensive unit tests for particle system

### Phase 3: Integration & High-Level Fx Class (Week 3) ✅ COMPLETE + REFACTORED
- [x] Create PolymetricBeats Fx2d class integrating BeatDetector + RhythmParticles
- [x] Wire all callbacks between components
- [x] Audio processing integration (processAudio method)
- [x] Rendering pipeline with configurable fade/clear-on-beat
- [x] Integration tests
- [x] **Refactor to separate algorithm from profile** (Post-Phase 3)

**Completed Files:**
- `src/fx/2d/polymetric_beats.h` - PolymetricBeats Fx class with unified API + PolymetricProfiles namespace
- `src/fx/2d/polymetric_beats.cpp` - Complete integration with automatic callback wiring
- `tests/test_polymetric_beats.cpp` - 11 integration tests covering all major functionality

### Phase 4: Example & Documentation (Week 4)
- [ ] Create `RhythmicParticles.ino` example (following `BeatDetection.ino` pattern)
- [ ] Add UI controls using existing UISlider, UINumberField, UICheckbox, UIGroup
- [ ] Write comprehensive documentation
- [ ] Performance benchmarks
- [ ] Integration tests with real audio

## Testing Strategy

### Unit Tests
- Polymetric phase alignment (7 vs 8 cycles)
- Swing quantization accuracy
- Per-band onset detection (verify bass/mid/high callbacks)
- Particle emission/culling
- SoA memory layout correctness

### Integration Tests
- End-to-end with synthetic 4/4 + syncopation
- Tempo change handling (existing tempo tracker robustness)
- Memory usage profiling
- Frame timing consistency

### Acceptance Criteria
1. **Accuracy:** Locks to 100-140 BPM ±1.5 BPM within 4 seconds
2. **Overlay:** `phase7_8` visibly drifts against `phase4_4`, realigns every 2 bars
3. **Performance:** 60 FPS sustained with 1000 particles on ESP32-S3
4. **Memory:** Static allocation only, ≤24KB overhead
5. **Visual:** Kick ducking visible within 20ms of onset

## File Structure (UPDATED)

```
src/fx/audio/beat_detector.h         [MODIFIED - added polymetric callbacks & config]
src/fx/audio/beat_detector.cpp       [MODIFIED - integrated PolymetricAnalyzer]
src/fx/audio/polymetric_analyzer.h   [NEW - generic N/M overlay tracker]
src/fx/audio/polymetric_analyzer.cpp [NEW - generic N/M overlay tracker]

src/fx/particles/rhythm_particles.h  [NEW - generic particle system]
src/fx/particles/rhythm_particles.cpp [NEW - generic particle system]

src/fx/2d/polymetric_beats.h         [NEW - integration Fx class + PolymetricProfiles namespace]
src/fx/2d/polymetric_beats.cpp       [NEW - integration implementation]

examples/Fx/RhythmicParticles/
  └── RhythmicParticles.ino           [FUTURE - demo with UISlider/UIGroup]

tests/
  ├── test_polymetric_beat.cpp        [NEW - polymetric analysis tests]
  ├── test_rhythm_particles.cpp       [NEW - particle system tests]
  └── test_polymetric_beats.cpp       [NEW - integration tests]

docs/fx/
  └── RhythmicParticles.md            [NEW - user guide]
```

## Dependencies

### Existing (Verified) ✅
- `beat_detector.{h,cpp}` - SuperFlux onset detection + tempo tracking
- `sound_to_midi.{h,cpp}` - FFT utilities for band splits
- `fl/ui.h` - UISlider, UINumberField, UICheckbox, UIGroup, UIAudio
- `fl::vector`, `fl::function`, `CRGB` types

### New Dependencies
- Curl noise function (2D/3D Perlin-based) - will need to implement or find existing in FastLED

## Open Questions

1. Should the 7/8 overlay be hardcoded or user-configurable (N/M over K bars)?
   - **Recommendation:** Make it configurable via `BeatDetectorConfig` fields

2. Particle collision detection: enable by default or opt-in?
   - **Recommendation:** Opt-in via `RhythmParticlesConfig` (disabled by default for performance)

3. Should we support multiple overlay patterns simultaneously (7/8 + 5/4)?
   - **Recommendation:** Phase 1 supports single overlay, add multiple overlays in future if needed

4. Bloom: GPU-style or simple box blur for MCU?
   - **Recommendation:** 1-tap exponential blur (fast on MCU)

## Key Changes from Original Spec

### What Already Exists ✅
1. **BeatDetector class** - Already has SuperFlux, multi-band onset detection, tempo tracking
2. **Callbacks:** `onOnset` and `onBeat` already implemented
3. **Multi-band analysis** - Already computed internally via `MultiBand` onset detection function
4. **UI system** - UISlider, UINumberField, UICheckbox, UIGroup already available
5. **Example structure** - BeatDetection.ino shows the pattern to follow

### What Needs to Be Added
1. **Polymetric callbacks** - Extend `BeatDetector` with new callbacks for 7/8 overlay, subdivisions, fills
2. **Per-band onset callbacks** - Expose existing multi-band analysis via `onOnsetBass/Mid/High` callbacks
3. **PolymetricAnalyzer** - New class to compute 7/8 overlay phase, swing timing, tuplet detection
4. **RhythmParticles** - New particle system with SoA layout and physics
5. **Example integration** - New `RhythmicParticles.ino` following existing patterns

### Terminology Updates
- ~~"Particle filter tempo tracking"~~ → Already exists as `TempoTrackerType::ParticleFilter` in BeatDetector
- ~~"Event bus"~~ → Use existing `fl::function` callbacks in `BeatDetector` class
- ~~"Multi-band onset callbacks"~~ → Extend existing `onOnset` to also fire per-band callbacks

---

**This task aligns with existing FastLED architecture while adding Tipper-specific rhythm analysis. All new code follows `fl::` namespace, uses existing types, and maintains zero-heap-allocation real-time constraints.**
