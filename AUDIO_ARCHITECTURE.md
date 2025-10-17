# FastLED Audio Architecture Design

**Version:** 3.0
**Date:** 2025-01-16
**Status:** Architectural organization and separation of concerns

---

## Table of Contents

1. [Executive Summary](#executive-summary)
2. [Architecture Overview](#architecture-overview)
3. [Directory Structure](#directory-structure)
4. [Low-Level Audio Infrastructure (fl/audio)](#low-level-audio-infrastructure-flaudio)
5. [High-Level Audio Effects (fx/audio)](#high-level-audio-effects-fxaudio)
6. [Migration Strategy](#migration-strategy)
7. [Design Principles](#design-principles)
8. [Future Roadmap](#future-roadmap)

---

## Executive Summary

This document defines the architectural separation between **low-level audio primitives** and **high-level audio effects** in the FastLED library:

- **`fl/audio/`** - Low-level audio infrastructure (primitives, context, base classes)
- **`fx/audio/`** - High-level audio effects and detectors (user-facing components)

### Key Design Goals

1. **Separation of Concerns** - Primitives vs. effects clearly separated
2. **Dependency Flow** - `fx/audio` depends on `fl/audio`, never the reverse
3. **Reusability** - Low-level components usable by any high-level detector
4. **Performance** - Shared computation via AudioContext pattern
5. **Extensibility** - Easy to add new detectors without modifying infrastructure

---

## Architecture Overview

### Two-Layer Architecture

```
┌─────────────────────────────────────────────────┐
│          USER APPLICATION LAYER                 │
│  (Arduino sketches, examples, user code)        │
└─────────────────────────────────────────────────┘
                      ↓ uses
┌─────────────────────────────────────────────────┐
│        HIGH-LEVEL AUDIO EFFECTS LAYER           │
│              fx/audio/detectors/                │
│                                                  │
│  • BeatDetector                                 │
│  • VocalDetector                                │
│  • PercussionDetector                           │
│  • ChordDetector                                │
│  • KeyDetector                                  │
│  • MoodAnalyzer                                 │
│  • BuildupDetector                              │
│  • DropDetector                                 │
│  • ... (17+ detectors)                          │
└─────────────────────────────────────────────────┘
                      ↓ depends on
┌─────────────────────────────────────────────────┐
│       LOW-LEVEL AUDIO INFRASTRUCTURE            │
│                 fl/audio/                       │
│                                                  │
│  • AudioContext (shared computation)            │
│  • AudioDetector (base class)                   │
│  • AudioProcessor (facade/orchestrator)         │
│  • Audio primitives (sample, FFT, etc.)         │
└─────────────────────────────────────────────────┘
                      ↓ uses
┌─────────────────────────────────────────────────┐
│         CORE FASTLED INFRASTRUCTURE             │
│              fl/ (core library)                 │
│                                                  │
│  • FFT, DSP primitives                          │
│  • Memory management (shared_ptr, etc.)         │
│  • Math utilities                               │
│  • Platform abstractions                        │
└─────────────────────────────────────────────────┘
```

### Dependency Flow

```
User Code
    ↓
fx/audio/detectors/*  ──→  fx/audio/beat.h
    ↓                              ↓
    ↓                      (high-level effects,
    ↓                       signal processing,
    ↓                       tempo tracking, etc.)
    ↓
fl/audio/*  ──────────→  fl/audio/audio_context.h
    ↓                    fl/audio/audio_detector.h
    ↓                    fl/audio/audio_processor.h
    ↓
    ↓          (low-level primitives, shared state,
    ↓           base classes, infrastructure)
    ↓
fl/*  ────────────────→  fl/fft.h, fl/ptr.h, fl/math.h, etc.
                         (core library primitives)
```

**Key Rule:** `fx/audio` can import from `fl/audio`, but `fl/audio` NEVER imports from `fx/audio`.

---

## Directory Structure

### Current State (Phase 1 - Completed)

```
src/
├── fl/
│   ├── audio/                      # ✅ Low-level infrastructure (NEW)
│   │   ├── audio_context.h/cpp     # Shared computation state (FFT caching)
│   │   ├── audio_detector.h        # Base class for all detectors
│   │   ├── audio_processor.h/cpp   # ❌ MISPLACED: Should be in fx/audio
│   │   └── README.md               # Infrastructure documentation
│   │
│   ├── audio.h/cpp                 # Audio input/sampling primitives
│   ├── audio_input.h/cpp           # Hardware audio input
│   ├── audio_reactive.h/cpp        # Legacy reactive components
│   ├── fft.h/cpp                   # FFT primitives
│   └── ...                         # Other core FL components
│
├── fx/
│   ├── audio/                      # High-level effects & detectors
│   │   ├── beat.h/cpp             # ✅ Existing (EDM beat detection)
│   │   ├── sound_to_midi.h/cpp     # ✅ Existing (pitch detection)
│   │   ├── README.md               # ✅ Existing (effects documentation)
│   │   │
│   │   └── detectors/              # ❌ TODO: New detector modules
│   │       ├── vocal.h/cpp
│   │       ├── percussion.h/cpp
│   │       ├── chord.h/cpp
│   │       ├── key.h/cpp
│   │       ├── mood_analyzer.h/cpp
│   │       ├── buildup.h/cpp
│   │       ├── drop.h/cpp
│   │       ├── tempo_analyzer.h/cpp
│   │       ├── frequency_bands.h/cpp
│   │       ├── energy_analyzer.h/cpp
│   │       ├── transient.h/cpp
│   │       ├── note.h/cpp
│   │       ├── downbeat.h/cpp
│   │       ├── dynamics_analyzer.h/cpp
│   │       ├── pitch.h/cpp
│   │       └── silence.h/cpp
│   │
│   ├── 1d/                         # 1D effects
│   ├── 2d/                         # 2D effects
│   └── ...
```

### Target State (Phase 2 - Migration Plan)

```
src/
├── fl/
│   ├── audio/                      # Low-level infrastructure ONLY
│   │   ├── audio_context.h/cpp     # ✅ Shared computation state
│   │   ├── audio_detector.h        # ✅ Base class interface
│   │   └── README.md               # Infrastructure docs
│   │
│   ├── audio.h/cpp                 # Audio primitives
│   ├── audio_input.h/cpp           # Hardware input
│   └── ...
│
├── fx/
│   ├── audio/
│   │   ├── audio_processor.h/cpp   # ✅ High-level facade/orchestrator
│   │   ├── README.md               # Effects documentation
│   │   │
│   │   ├── detectors/              # ✅ All detector implementations
│   │   │   ├── beat.h/cpp
│   │   │   ├── vocal.h/cpp
│   │   │   ├── percussion.h/cpp
│   │   │   ├── chord.h/cpp
│   │   │   ├── key.h/cpp
│   │   │   ├── mood_analyzer.h/cpp
│   │   │   ├── buildup.h/cpp
│   │   │   ├── drop.h/cpp
│   │   │   ├── tempo_analyzer.h/cpp
│   │   │   ├── frequency_bands.h/cpp
│   │   │   ├── energy_analyzer.h/cpp
│   │   │   ├── transient.h/cpp
│   │   │   ├── note.h/cpp
│   │   │   ├── downbeat.h/cpp
│   │   │   ├── dynamics_analyzer.h/cpp
│   │   │   ├── pitch.h/cpp
│   │   │   └── silence.h/cpp
│   │   │
│   │   └── advanced/               # ✅ Advanced signal processing
│   │       ├── sound_to_midi.h/cpp
│   │       └── ... (future advanced modules)
│   │
│   ├── 1d/
│   ├── 2d/
│   └── ...
```

---

## Low-Level Audio Infrastructure (fl/audio)

### Purpose

Provides **reusable primitives** for building audio detectors:

- **Shared computation state** (AudioContext)
- **Base classes and interfaces** (AudioDetector)
- **Facade pattern** for ease-of-use (AudioProcessor)
- **No domain-specific logic** (no beat detection, no chord analysis)

### Components

#### 1. AudioContext (audio_context.h/cpp)

**What it is:** Shared computation state with lazy evaluation and caching.

**Responsibilities:**
- Wrap audio sample with metadata (timestamp, RMS, ZCF)
- Lazy FFT computation with caching (computed once, shared by all detectors)
- FFT history ring buffer for temporal analysis
- Clear separation: NO detector-specific logic

**Key Methods:**
```cpp
class AudioContext {
    // Sample access
    const AudioSample& getSample() const;
    float getRMS() const;
    float getZCF() const;

    // Lazy FFT (cached)
    const FFTBins& getFFT(int bands = 16, float fmin = 20, float fmax = 8000);
    bool hasFFT() const;

    // FFT history for temporal analysis
    const vector<FFTBins>& getFFTHistory(int depth = 4);
    const FFTBins* getHistoricalFFT(int framesBack) const;

    // State management
    void setSample(const AudioSample& sample);
    void clearCache();
};
```

**Design Principle:** Pure infrastructure - no beats, no chords, no moods. Just shared computation.

#### 2. AudioDetector (audio_detector.h)

**What it is:** Abstract base class for all audio detectors.

**Responsibilities:**
- Define common interface for all detectors
- Declare virtual methods for update, reset, etc.
- Metadata (name, FFT requirements)

**Key Methods:**
```cpp
class AudioDetector {
public:
    virtual ~AudioDetector() = default;

    // Core interface
    virtual void update(shared_ptr<AudioContext> context) = 0;
    virtual void reset() {}

    // Metadata
    virtual const char* getName() const = 0;
    virtual bool needsFFT() const { return false; }
    virtual bool needsFFTHistory() const { return false; }
};
```

**Design Principle:** Minimal interface - just enough for polymorphism and orchestration.

### What Does NOT Belong in fl/audio

❌ **Beat detection algorithms** → Goes in `fx/audio/detectors/beat.h`
❌ **Vocal analysis** → Goes in `fx/audio/detectors/vocal.h`
❌ **Chord recognition** → Goes in `fx/audio/detectors/chord.h`
❌ **Tempo tracking** → Goes in `fx/audio/detectors/tempo_analyzer.h`
❌ **AudioProcessor facade** → Goes in `fx/audio/audio_processor.h` (high-level orchestration)
❌ **ANY domain-specific audio analysis** → Goes in `fx/audio/detectors/*`

**Only low-level primitives and base classes belong in `fl/audio`.**

---

## High-Level Audio Effects (fx/audio)

### Purpose

Provides **user-facing audio detectors and effects**:

- **High-level facade** (AudioProcessor for easy orchestration)
- **Domain-specific algorithms** (beat detection, vocal detection, etc.)
- **Signal processing** (spectral flux, onset detection, etc.)
- **Musical analysis** (tempo, key, chord, mood, etc.)
- **Callback-based events** (onBeat, onVocal, onChord, etc.)

### Organization

#### fx/audio/audio_processor.h/cpp

**What it is:** High-level facade that orchestrates detectors and provides simple API.

**Responsibilities:**
- Lazy instantiation of detectors
- Manage shared AudioContext
- Provide callback-based event API
- Route events to/from detectors

**Key Methods:**
```cpp
class AudioProcessor {
public:
    // Main update
    void update(const AudioSample& sample);

    // Event registration (examples)
    void onBeat(function<void()> callback);
    void onVocal(function<void(bool active)> callback);
    void onPercussion(function<void(const char* type)> callback);
    // ... (one method per detector type)

    // State access
    shared_ptr<AudioContext> getContext() const;
    void reset();

private:
    shared_ptr<AudioContext> mContext;

    // Lazy detector storage
    shared_ptr<BeatDetector> mBeatDetector;
    shared_ptr<VocalDetector> mVocalDetector;
    // ... (one per detector type)
};
```

**Design Principle:** Simple, callback-based API for end users. Hides complexity of detector management.

**Why in fx/audio?** AudioProcessor is a high-level orchestrator that knows about specific detector types (BeatDetector, VocalDetector, etc.). It's user-facing and provides convenience, not low-level infrastructure.

#### fx/audio/detectors/

All music/audio detection implementations:

| Detector | Purpose | Key Algorithms |
|----------|---------|----------------|
| **beat** | Rhythmic pulse detection | SuperFlux ODF, comb filter tempo tracking |
| **vocal** | Human voice detection | Spectral centroid, formant ratio, rolloff |
| **percussion** | Drum hit detection | Multi-band flux, kick/snare/hihat classification |
| **chord** | Chord recognition | Pitch class profile, harmonic matching |
| **key** | Musical key detection | Krumhansl-Schmuckler algorithm |
| **mood_analyzer** | Emotional content | Circumplex model (valence/arousal) |
| **buildup** | EDM buildup detection | Energy slope, spectral bandwidth increase |
| **drop** | EDM drop detection | Energy spike, bass impact |
| **tempo_analyzer** | BPM tracking | Autocorrelation, Rayleigh weighting |
| **frequency_bands** | Bass/mid/treble | Mel-scale band energy |
| **energy_analyzer** | RMS, peak tracking | Time-domain envelope |
| **transient** | Attack detection | Onset strength function |
| **note** | MIDI note detection | Pitch to MIDI conversion |
| **downbeat** | Measure-level timing | Beat phase, meter detection |
| **dynamics_analyzer** | Crescendo/diminuendo | Loudness trend analysis |
| **pitch** | Pitch tracking | YIN/MPM autocorrelation |
| **silence** | Auto-standby | RMS threshold, duration tracking |

#### fx/audio/advanced/

Advanced signal processing modules:

| Module | Purpose |
|--------|---------|
| **sound_to_midi** | Monophonic/polyphonic pitch→MIDI conversion |
| ... (future modules) | Advanced DSP, machine learning, etc. |

### Detector Implementation Pattern

All detectors follow this pattern:

```cpp
// fx/audio/detectors/example.h
#pragma once

#include "fl/audio/audio_detector.h"
#include "fl/audio/audio_context.h"
#include "fl/function.h"

namespace fl {

class ExampleDetector : public AudioDetector {
public:
    ExampleDetector();
    ~ExampleDetector() override;

    // AudioDetector interface
    void update(shared_ptr<AudioContext> context) override;
    bool needsFFT() const override { return true; }
    const char* getName() const override { return "ExampleDetector"; }
    void reset() override;

    // Callbacks
    function<void(float value)> onEvent;

    // Configuration
    void setThreshold(float threshold);

    // State access
    float getCurrentValue() const;

private:
    // Detector-specific state
    float mThreshold;
    float mCurrentValue;

    // Detector-specific algorithms
    float analyzeSignal(const FFTBins& fft);
};

} // namespace fl
```

**Key Pattern Elements:**
1. ✅ Inherit from `AudioDetector`
2. ✅ Accept `shared_ptr<AudioContext>` in `update()`
3. ✅ Use `context->getFFT()` to access shared FFT (lazy, cached)
4. ✅ Provide callbacks for events (`function<void(...)>`)
5. ✅ Implement domain-specific algorithms
6. ✅ Store detector-specific state privately

---

## Migration Strategy

### Phase 1: Infrastructure (✅ COMPLETE)

**Status:** All infrastructure is in place in `fl/audio/`:

- ✅ AudioContext with lazy FFT caching
- ✅ AudioDetector base class
- ✅ AudioProcessor facade
- ✅ Documentation in `fl/audio/README.md`

**Current detector locations:**
- ✅ `src/fl/audio/beat.h/cpp` (17 detectors implemented)
- ✅ `src/fl/audio/vocal.h/cpp`
- ✅ `src/fl/audio/percussion.h/cpp`
- ... (all 17 detectors currently in `fl/audio`)

### Phase 2: Reorganization (❌ TODO - NEXT STEP)

**Goal:** Move all detector implementations to `fx/audio/detectors/`.

**Step 1: Create detector directory**
```bash
mkdir -p src/fx/audio/detectors
```

**Step 2: Move AudioProcessor to fx/audio**
```bash
# Move AudioProcessor from fl/audio to fx/audio (it's a high-level facade)
mv src/fl/audio/audio_processor.{h,cpp} src/fx/audio/
```

**Step 3: Move detector files to fx/audio/detectors**
```bash
# Move all detector and analyzer files from fl/audio to fx/audio/detectors
mv src/fl/audio/beat.{h,cpp} src/fx/audio/detectors/
mv src/fl/audio/vocal.{h,cpp} src/fx/audio/detectors/
mv src/fl/audio/percussion.{h,cpp} src/fx/audio/detectors/
mv src/fl/audio/chord.{h,cpp} src/fx/audio/detectors/
mv src/fl/audio/key.{h,cpp} src/fx/audio/detectors/
mv src/fl/audio/mood_analyzer.{h,cpp} src/fx/audio/detectors/
mv src/fl/audio/buildup.{h,cpp} src/fx/audio/detectors/
mv src/fl/audio/drop.{h,cpp} src/fx/audio/detectors/
mv src/fl/audio/tempo_analyzer.{h,cpp} src/fx/audio/detectors/
mv src/fl/audio/frequency_bands.{h,cpp} src/fx/audio/detectors/
mv src/fl/audio/energy_analyzer.{h,cpp} src/fx/audio/detectors/
mv src/fl/audio/transient.{h,cpp} src/fx/audio/detectors/
mv src/fl/audio/note.{h,cpp} src/fx/audio/detectors/
mv src/fl/audio/downbeat.{h,cpp} src/fx/audio/detectors/
mv src/fl/audio/dynamics_analyzer.{h,cpp} src/fx/audio/detectors/
mv src/fl/audio/pitch.{h,cpp} src/fx/audio/detectors/
mv src/fl/audio/silence.{h,cpp} src/fx/audio/detectors/
```

**Step 4: Update includes in moved files**
```cpp
// In detector files - no change needed:
#include "fl/audio/audio_detector.h"  // Still correct!
#include "fl/audio/audio_context.h"   // Still correct!
```

**Step 5: Update includes in AudioProcessor**
```cpp
// In fx/audio/audio_processor.cpp
// Change:
#include "fl/audio/beat.h"

// To:
#include "fx/audio/detectors/beat.h"
```

**Step 6: Update user-facing includes**
```cpp
// In user sketches and examples:
// Change:
#include "fl/audio/audio_processor.h"

// To:
#include "fx/audio/audio_processor.h"
```

**Step 7: Update documentation**
- Update `fl/audio/README.md` to reflect infrastructure-only (AudioContext, AudioDetector)
- Update `fx/audio/README.md` to document AudioProcessor and all detectors
- Update examples to use new paths

**Step 8: Run tests and validation**
```bash
uv run test.py                # All tests must pass
bash lint                     # Linting must pass
bash compile uno --examples   # Examples must compile
```

### Phase 3: Advanced Modules (Future)

Move advanced signal processing to `fx/audio/advanced/`:

```bash
mv src/fx/audio/sound_to_midi.{h,cpp} src/fx/audio/advanced/
```

### Phase 4: Cleanup (Future)

Once migration is complete:

1. Verify `fl/audio/` contains ONLY:
   - `audio_context.h/cpp` ✅ (shared FFT caching)
   - `audio_detector.h` ✅ (base class interface)
   - `README.md` ✅ (infrastructure docs)

2. Verify `fx/audio/` contains:
   - `audio_processor.h/cpp` ✅ (high-level facade)
   - `detectors/` (all 17+ detectors)
   - `advanced/` (sound_to_midi, etc.)
   - `README.md` (updated documentation)

3. Update all examples and documentation

---

## Design Principles

### 1. Separation of Concerns

**Infrastructure vs. Implementation**

- **`fl/audio`** = Infrastructure (shared state, base classes, facades)
- **`fx/audio`** = Implementation (detectors, effects, algorithms)

### 2. Dependency Flow

**One-way dependency:**

```
fx/audio → fl/audio → fl/core
```

**NEVER:**
```
fl/audio → fx/audio  ❌ FORBIDDEN
```

### 3. Reusability

**Low-level components must be reusable:**

- AudioContext used by ALL detectors
- AudioDetector base class for ALL detector types
- AudioProcessor orchestrates ANY detector

### 4. Performance

**Shared computation via AudioContext:**

```cpp
// FFT computed once
AudioContext ctx(sample);

// Multiple detectors share the same FFT (cached)
beatDetector.update(ctx);     // Computes FFT once
vocalDetector.update(ctx);    // Reuses cached FFT ✅
percussionDetector.update(ctx); // Reuses cached FFT ✅
```

**Result:** 3× speedup when 3 detectors use FFT (vs. computing 3 times).

### 5. Extensibility

**Easy to add new detectors:**

1. Create new detector class in `fx/audio/detectors/`
2. Inherit from `AudioDetector`
3. Implement `update()` using `context->getFFT()`
4. Register in `AudioProcessor` (if using facade)
5. Done!

---

## Future Roadmap

### Phase 1: Infrastructure ✅ COMPLETE

- ✅ AudioContext with lazy FFT caching
- ✅ AudioDetector base class
- ✅ AudioProcessor facade
- ✅ 17 detectors implemented

### Phase 2: Reorganization (NEXT)

- ❌ Move detectors to `fx/audio/detectors/`
- ❌ Move advanced modules to `fx/audio/advanced/`
- ❌ Update documentation and examples
- ❌ Validate with tests and compilation

### Phase 3: Additional Detectors (Future)

From the complete catalog in `NEW_AUDIO_DESIGN.md`:

**Tier 4: Specialized (Optional)**
- Reverb detector
- Compression detector
- Sidechain detector
- Genre classifier
- Danceability analyzer
- Section detector
- Harmonic complexity analyzer

**Tier 5: Advanced (Future)**
- Machine learning models
- Multi-channel analysis
- Spatial audio processing
- Real-time remixing

### Phase 4: Optimization (Future)

- Profile hot paths
- SIMD optimizations
- Platform-specific tuning
- Reduce memory footprint

### Phase 5: Documentation (Future)

- Comprehensive user guide
- API reference
- Algorithm deep-dives
- Tutorial videos

---

## Summary

### Current State

```
fl/audio/               ← Infrastructure + Detectors + Processor (mixed) ❌
  ├── audio_context     ← ✅ Belongs here
  ├── audio_detector    ← ✅ Belongs here
  └── audio_processor   ← ❌ Should be in fx/audio

fx/audio/               ← beat_detector, sound_to_midi only
```

### Target State

```
fl/audio/               ← Infrastructure ONLY ✅
  ├── audio_context     ← Shared FFT caching
  └── audio_detector    ← Base class interface

fx/audio/
  ├── audio_processor   ← High-level facade ✅
  ├── detectors/        ← All 17+ detectors ✅
  │   ├── beat
  │   ├── vocal
  │   └── ...
  └── advanced/         ← sound_to_midi, etc. ✅
```

### Migration Benefits

1. ✅ **Clear separation** - Infrastructure vs. implementation
2. ✅ **Better organization** - Detectors grouped logically
3. ✅ **Easier to find** - `fx/audio/detectors/vocal.h` is obvious
4. ✅ **Scalable** - Easy to add 50+ more detectors without clutter
5. ✅ **Maintainable** - Clear dependency flow, no circular deps

### Next Steps

1. Create `fx/audio/detectors/` directory
2. Move all detector files from `fl/audio` to `fx/audio/detectors/`
3. Update includes in `AudioProcessor`
4. Update documentation
5. Run tests and validate

---

**Document Version:** 3.0
**Date:** 2025-01-16
**Status:** Architecture defined, migration plan ready for execution
