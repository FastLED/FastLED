# FastLED Audio System - Agent Development Loop

## Current Status

**Phase:** Active development of new audio architecture (v2.0)
**Target:** Implement comprehensive audio processing system with shared FFT computation and modular detectors
**Location:** All audio code lives in `src/fl/audio/` for organization

---

## Development Loop Instructions

### 1. Read the Design Document

**Start here on each iteration:**
- Read `NEW_AUDIO_DESIGN.md` to understand the architecture
- Key concepts:
  - **AudioContext** pattern for shared computation (FFT computed once, reused by all detectors)
  - **AudioDetector** base class for modular detector implementation
  - **AudioProcessor** high-level facade with callback-based API
  - Lazy evaluation and lazy instantiation for efficiency

### 2. Identify Next Task

**Current implementation priorities (in order):**

#### Phase 1: Foundation (Tier 1 - 6 Core Detectors)
- [ ] `AudioContext` class with lazy FFT computation and history tracking
- [ ] `AudioDetector` base class interface
- [ ] `AudioProcessor` high-level facade with lazy detector instantiation
- [ ] `BeatDetector` - rhythmic pulse detection (most requested feature!)
- [ ] `FrequencyBands` - bass/mid/treble abstraction
- [ ] `EnergyAnalyzer` - overall loudness/RMS tracking
- [ ] `TempoAnalyzer` - BPM tracking with confidence

#### Phase 2: Enhancement (Tier 2 - 6 Important Features)
- [ ] `TransientDetector` - attack detection
- [ ] `NoteDetector` - musical note detection
- [ ] `DownbeatDetector` - measure-level timing
- [ ] `DynamicsAnalyzer` - loudness trends
- [ ] `PitchDetector` - pitch tracking
- [ ] `SilenceDetector` - auto-standby/silence detection

#### Phase 3: Differentiators (Tier 3 - 7 Unique Features)
- [ ] `VocalDetector` - human voice detection (unique!)
- [ ] `ChordDetector` - chord recognition (unique!)
- [ ] `KeyDetector` - musical key detection (unique!)
- [ ] `MoodAnalyzer` - mood/emotion detection (unique!)
- [ ] `PercussionDetector` - drum-specific detection (kick/snare/hihat)
- [ ] `BuildupDetector` - EDM buildup detection
- [ ] `DropDetector` - EDM drop detection

**Reference implementations available in NEW_AUDIO_DESIGN.md:**
- Complete `VocalDetector` example (lines 498-660)
- Complete `PercussionDetector` example (lines 665-843)

### 3. Implementation Guidelines

**File Organization:**
```
src/fl/audio/
├── audio_context.h/.cpp        # Shared computation state
├── audio_detector.h            # Base class interface
├── audio_processor.h/.cpp      # High-level facade
└── detectors/
    ├── beat.h/.cpp
    ├── vocal.h/.cpp
    ├── percussion.h/.cpp
    ├── frequency_bands.h/.cpp
    └── ... (other detectors)
```

**Code Standards:**
- Use `fl::` namespace instead of `std::`
- Check `fl/type_traits.h` before using stdlib headers like `<type_traits>`
- Use `FL_DBG("message" << var)` for debug output (stream-style, not printf)
- Use `FL_WARN("message" << var)` for warnings
- Follow naming conventions in existing codebase
- Fully type annotate Python collections: `List[T]`, `Dict[K, V]`, `Set[T]`

**Testing Requirements:**
- Write unit tests for each new component
- Test FFT caching behavior in AudioContext
- Test lazy instantiation in AudioProcessor
- Test shared context between multiple detectors
- Integration tests for complete workflows

### 4. Testing & Validation

**After each implementation:**
```bash
# Run all tests
uv run test.py

# Run C++ tests only
uv run test.py --cpp

# Run specific test
uv run test.py AudioContextTest

# Disable fingerprint caching (force rebuild)
uv run test.py --no-fingerprint

# Run linting
bash lint
```

**For JavaScript changes:**
```bash
bash lint --js
```

### 5. Memory Refresh

**Before concluding work:**
- Re-read `NEW_AUDIO_DESIGN.md` to ensure alignment with architecture
- Re-read this file (`LOOP.md`) to refresh on current priorities
- Review relevant `AGENTS.md` files:
  - `ci/AGENTS.md` for build system tasks
  - `tests/AGENTS.md` for test requirements
  - `examples/AGENTS.md` for Arduino sketch rules

---

## Key Architecture Patterns

### AudioContext Pattern (Core Innovation)

```cpp
// Create lightweight context wrapping audio sample
AudioContext ctx(sample);

// Multiple detectors access same context
beatDetector.update(ctx);      // Computes FFT once
vocalDetector.update(ctx);     // Reuses cached FFT!
percussionDetector.update(ctx); // Reuses cached FFT!
```

**Benefits:**
- FFT computed once and shared between all detectors
- Lazy evaluation - only computed when needed
- Minimal complexity - simple context object
- Optimal performance

### Detector Implementation Pattern

```cpp
class MyDetector : public AudioDetector {
public:
    // Override virtual methods
    void update(shared_ptr<AudioContext> context) override;
    bool needsFFT() const override { return true; }
    const char* getName() const override { return "MyDetector"; }
    void reset() override;

    // Callbacks for events
    function<void()> onEvent;

private:
    // Detector state
};
```

### AudioProcessor Usage Pattern

```cpp
// Simple high-level API
AudioProcessor processor;

processor.onBeat([]() {
    // React to beat
});

void loop() {
    while (AudioSample sample = audio.next()) {
        processor.update(sample);  // Updates all registered detectors
    }
}
```

---

## Performance Targets

**Expected performance:**
- `AudioContext::getFFT()` first call: ~2-5ms
- `AudioContext::getFFT()` cached: ~0.001ms (pointer return)
- Detector `update()`: ~0.1-0.5ms per detector
- Total `AudioProcessor::update()`: ~2-6ms with 3-5 active detectors

**Memory footprint:**
- `AudioProcessor`: ~40 bytes (stack)
- `AudioContext`: ~200 bytes (heap, shared)
- Per detector: ~50-100 bytes (heap, lazy)
- FFT cache (16 bands): ~128 bytes
- FFT history (4 frames): ~512 bytes
- **Total (5 detectors): ~1.2 KB**

---

## Git & Commit Policy

**IMPORTANT: Do NOT commit code unless explicitly requested by user**
- User controls all git operations
- Never run `git commit` or `git push` without explicit instruction
- Never run `codeup` unless user says "run codeup" or "codeup"

---

## Quick Reference Commands

```bash
# Testing
uv run test.py                      # All tests
uv run test.py --cpp                # C++ only
uv run test.py TestName             # Specific test
uv run test.py --no-fingerprint     # Force rebuild

# Linting
bash lint                           # C++ linting
bash lint --js                      # JavaScript linting

# Compilation
uv run ci/ci-compile.py uno --examples Blink
uv run ci/wasm_compile.py examples/Blink --just-compile

# Code Search
/git-historian keyword1 keyword2 --paths src/fl/audio
```

---

## Notes for AI Agents

1. **Always read NEW_AUDIO_DESIGN.md first** - it contains the complete architecture specification
2. **Follow the phased approach** - implement Tier 1 before Tier 2, etc.
3. **Test after each implementation** - run `uv run test.py` and `bash lint`
4. **Use reference implementations** - VocalDetector and PercussionDetector are complete examples
5. **Stay in project root** - never `cd` to subdirectories
6. **Use `uv run python`** - never just `python`
7. **Refresh memory before concluding** - re-read this file and NEW_AUDIO_DESIGN.md

---

**Document Version:** 1.0
**Date:** 2025-01-16
**Status:** Active development loop for audio system v2.0
