# Cleanup & Optimization Guide - Polymetric Beat Visualization

## Overview

This document details all changes made during the polymetric beat visualization implementation and provides guidance for future cleanup, optimization, and refactoring work.

**Implementation Date**: 2025-10-05
**Phases Completed**: 1-3 (Core functionality)
**Phase Status**: Refactoring required before commit

---

## 🚨 CRITICAL ARCHITECTURE ISSUE - Must Fix Before Commit

### Problem: "Tipper" Is a Profile, Not a Visualizer

**Current Issue:**
- `TipperBeats` is implemented as an Fx2d visualizer class
- This conflates a **generic polymetric beat detection algorithm** with **Tipper-specific parameters**
- "Tipper" should be a **configuration profile/preset**, not a class name

**Root Cause:**
- The polymetric analyzer is generic (handles any N/M overlay patterns)
- The particle system is generic (responds to any beat detection events)
- Only the **default configuration values** are Tipper-specific (7/8 overlay, specific swing amounts, etc.)

### Solution: Rename & Restructure

**Required Changes:**

1. **Rename `TipperBeats` → `PolymetricBeats`** (generic name for the visualizer)
   - File: `src/fx/2d/tipper_beats.h` → `src/fx/2d/polymetric_beats.h`
   - File: `src/fx/2d/tipper_beats.cpp` → `src/fx/2d/polymetric_beats.cpp`
   - Class: `TipperBeats` → `PolymetricBeats`
   - Config: `TipperBeatsConfig` → `PolymetricBeatsConfig`

2. **Extract "Tipper" as a Configuration Profile/Preset**
   - Create namespace `PolymetricProfiles` with presets:
     ```cpp
     namespace PolymetricProfiles {
         // Tipper-style broken beat EDM (7/8 over 4/4)
         inline PolymetricBeatsConfig Tipper() {
             PolymetricBeatsConfig cfg;
             cfg.beat_cfg.polymetric.overlay_numerator = 7;
             cfg.beat_cfg.polymetric.overlay_bars = 2;
             cfg.beat_cfg.polymetric.swing_amount = 0.12f;
             // ... other Tipper-specific values
             return cfg;
         }

         // Future profiles:
         // inline PolymetricBeatsConfig Aphex() { ... }  // 5/4 + triplets
         // inline PolymetricBeatsConfig Amon() { ... }   // Complex polyrhythms
     }
     ```

3. **Update All References**
   - Tests: `test_tipper_beats.cpp` → `test_polymetric_beats.cpp`
   - Examples: Use `PolymetricBeats fx(map, PolymetricProfiles::Tipper());`
   - Documentation: Emphasize generic algorithm with Tipper as one example profile

### Why This Matters

**Correctness:**
- The algorithm is **already generic** - it handles arbitrary N/M overlays, not just 7/8
- "Tipper" getting embedded in the class name falsely implies the algorithm is Tipper-specific
- This would confuse users trying to use the system for other percussion styles (Aphex Twin, Amon Tobin, etc.)

**Extensibility:**
- With profiles, users can easily create presets for different artists/styles
- Generic naming allows the system to grow beyond one use case
- Separates **what** the algorithm does from **how** it's configured

**API Clarity:**
- `PolymetricBeats` clearly describes what the class does (polymetric beat visualization)
- `TipperProfile` clearly describes a **preset configuration**, not an algorithm

---

## Files Added/Modified (Post-Refactoring)

### Audio Analysis (2 new files)
1. **`src/fx/audio/polymetric_analyzer.h`** (135 lines) ✅ **GENERIC - No changes needed**
   - Generic class for N/M overlay tracking, swing/humanize micro-timing
   - Dependencies: `fl/function.h`, `fl/stdint.h`
   - Memory: ~100 bytes per instance (small state)
   - **Status**: Already generic, works for any polymetric pattern

2. **`src/fx/audio/polymetric_analyzer.cpp`** (166 lines) ✅ **GENERIC - No changes needed**
   - Implementation of polymetric rhythm analysis
   - Uses `inoise16_raw` for noise generation
   - **Status**: Already generic, no Tipper-specific logic

### Particle System (2 new files)
3. **`src/fx/particles/rhythm_particles.h`** (213 lines) ✅ **GENERIC - No changes needed**
   - SoA particle system with 4 configurable emitters
   - Configuration structures: `RhythmParticlesConfig`, `ParticleEmitterConfig`
   - **Status**: Already generic, emitter configs are user-provided
   - **Cleanup target**: Large class, consider splitting emitter logic into separate class

4. **`src/fx/particles/rhythm_particles.cpp`** (496 lines) ✅ **GENERIC - No changes needed**
   - Complete particle system implementation
   - **Status**: Already generic
   - **Cleanup targets**:
     - Simple LCG RNG (line 202-210) - consider replacing with better RNG
     - Manual `new[]`/`delete[]` allocation - could use `fl::vector` instead
     - Curl noise calculation could be optimized (3 separate methods)

### Integration Layer (2 new files) 🚨 **REQUIRES REFACTORING**
5. **`src/fx/2d/polymetric_beats.h`** (156 lines) - **RENAMED from `tipper_beats.h`**
   - High-level Fx2d integration class
   - Class: `PolymetricBeats` (was `TipperBeats`)
   - Config: `PolymetricBeatsConfig` (was `TipperBeatsConfig`)
   - **Changes required**:
     - Remove Tipper-specific defaults from constructor
     - Make all config values explicit (no hidden defaults)
     - Add `PolymetricProfiles` namespace with preset functions
   - **Cleanup target**: Config comparison in `setConfig()` could be improved

6. **`src/fx/2d/polymetric_beats.cpp`** (139 lines) - **RENAMED from `tipper_beats.cpp`**
   - Integration implementation with callback wiring
   - **Changes required**:
     - Update class name to `PolymetricBeats`
     - Remove Tipper-specific defaults
   - **Cleanup target**: Lambda captures could be optimized (currently capturing `this`)

### Tests (4 test files) 🚨 **REQUIRES UPDATES**
7. **`tests/test_polymetric_beat.cpp`** (192 lines) ✅ **No changes needed**
   - 9 tests for polymetric analysis (already generic)
   - **Note**: One test uses relaxed check (subdivision detection)

8. **`tests/test_rhythm_particles.cpp`** (248 lines) ✅ **No changes needed**
   - 15 tests for particle system (already generic)
   - **Cleanup target**: Some tests verify "doesn't crash" - could add more assertions

9. **`tests/test_polymetric_beats.cpp`** (230 lines) - **RENAMED from `test_tipper_beats.cpp`**
   - 11 integration tests
   - **Changes required**:
     - Update all `TipperBeats` → `PolymetricBeats`
     - Update all `TipperBeatsConfig` → `PolymetricBeatsConfig`
     - Consider adding test with `PolymetricProfiles::Tipper()`
   - **Cleanup target**: Background fade test simplified to avoid flakiness

10. **`tests/test_beat_detector_benchmark.cpp`** ✅ **No changes needed**
    - Performance benchmarking (already generic)

---

## Files Modified (2 Existing Files)

### `src/fx/audio/beat_detector.h`
**Changes Made**:
- Added `#include "polymetric_analyzer.h"` (line 55)
- Added `MultiBandOnset` struct (lines 158-163)
- Added 3 onset callbacks: `onOnsetBass`, `onOnsetMid`, `onOnsetHigh` (lines 419-433)
- Added 3 polymetric callbacks: `onPolymetricBeat`, `onSubdivision`, `onFill` (lines 435-448)
- Added 3 phase accessor methods (lines 491-501)
- Added `PolymetricAnalyzer _polymetricAnalyzer` member (line 510)
- Added 3 per-band peak pickers (lines 513-515)
- Added `processMultiBandOnsets()` method declaration (line 527)

**Impact**: ~50 lines added to header
**Cleanup Opportunities**:
- Consider moving `MultiBandOnset` to separate header if reused elsewhere
- Per-band peak pickers could be array instead of 3 separate members

### `src/fx/audio/beat_detector.cpp`
**Changes Made**:
- Updated constructor to initialize polymetric analyzer and per-band pickers (lines 1089-1091)
- Added callback wiring in constructor (lines 1101-1118)
- Updated `reset()` to reset polymetric analyzer (line 1128)
- Updated `setConfig()` to configure polymetric analyzer (line 1144)
- Modified `computeMultiBand()` to track per-band onset values (lines 241-273)
- Added `processMultiBandOnsets()` calls in both `processFrame()` and `processSpectrum()` (lines 1192, 1234)
- Implemented `processMultiBandOnsets()` method (lines 1272-1304)
- Implemented 3 phase accessor methods (lines 1306-1316)
- Added `getLastMultiBandOnset()` method to `OnsetDetectionProcessor` (line in header)

**Impact**: ~100 lines added to implementation
**Cleanup Opportunities**:
- `processMultiBandOnsets()` assumes exactly 3 bands - could be more generic
- Callback wiring uses lambdas - consider if performance impact is acceptable
- Band index iteration (line 253) uses hardcoded 0/1/2 - should use enum or constants

---

## Architecture & Design Decisions

### Memory Management
- **SoA Pattern**: Particle system uses Structure-of-Arrays for cache efficiency
  - Pro: Better cache locality during physics updates
  - Con: More complex allocation/deallocation
  - **Cleanup**: Consider using `fl::vector` instead of manual `new[]`/`delete[]`

### Callback System
- **Lambda Captures**: Extensive use of `[this]` lambda captures for wiring
  - Pro: Clean, maintainable code
  - Con: Slight overhead per callback
  - **Cleanup**: Measure performance, consider function pointers if needed

### RNG Implementation
- **Simple LCG**: Uses basic Linear Congruential Generator
  - Pro: Fast, deterministic
  - Con: Poor statistical quality
  - **Cleanup**: Replace with `random16()` from FastLED or better PRNG

---

## Known Issues & Technical Debt

### 1. Hardcoded Band Indices
**Location**: `beat_detector.cpp:253-263`
```cpp
// Assumes default 3-band config: bass, mid, high
if (band_idx == 0) {
    _lastMultiBandOnset.bass = band_flux;
} else if (band_idx == 1) {
    _lastMultiBandOnset.mid = band_flux;
} else if (band_idx == 2) {
    _lastMultiBandOnset.high = band_flux;
}
```
**Fix**: Use enum or dynamic band mapping

### 2. Manual Memory Management
**Location**: `rhythm_particles.cpp:104-147`
```cpp
void RhythmParticles::allocateParticles(int max_particles) {
    _x = new float[max_particles];
    _y = new float[max_particles];
    // ... etc
}
```
**Fix**: Use `fl::vector` or fixed-size arrays with template parameter

### 3. Config Comparison
**Location**: `tipper_beats.cpp:108-112`
```cpp
// Check what changed
if (&cfg.beat_cfg != &_cfg.beat_cfg) {
    detector_changed = true;
}
```
**Fix**: Pointer comparison doesn't check actual content changes - should compare values

### 4. Static Variables Removed
**Location**: Previously in `polymetric_analyzer.cpp:105`
- Static variable `last_phase_16th` was replaced with instance member `_lastPhase16th`
- This was necessary for proper test isolation
- **Good**: Tests now properly isolated between runs

---

## Performance Optimization Opportunities

### 1. Particle Culling (Medium Priority)
**Location**: `rhythm_particles.cpp:334-343`
- Current: O(n) scan through all particles
- **Optimization**: Use free list or swap-and-pop pattern
- **Impact**: ~10-20% faster culling for 1000+ particles

### 2. Curl Noise Calculation (Low Priority)
**Location**: `rhythm_particles.cpp:356-408`
- Current: 3 separate methods, multiple `inoise16_raw` calls
- **Optimization**: Batch noise calculations, cache results
- **Impact**: ~5-10% faster physics updates

### 3. LED Rendering (Medium Priority)
**Location**: `rhythm_particles.cpp:413-445`
- Current: No spatial indexing for 2D grid
- **Optimization**: Use quad-tree or grid cells for particle lookup
- **Impact**: Significant for large grids (32x32+)

### 4. Phase Calculation (Low Priority)
**Location**: `polymetric_analyzer.cpp:89-102`
- Current: Recalculates phases every frame
- **Optimization**: Cache if timestamp hasn't changed
- **Impact**: Minimal (already very fast)

---

## Code Quality Improvements

### 1. Magic Numbers
**Scattered throughout code**
- `0.12f` (swing amount default)
- `0.35f` (kick duck amount)
- `100` (bloom threshold)
- **Fix**: Create named constants or config defaults

### 2. Error Handling
**Missing in several places**
- No validation for invalid particle counts
- No checks for null LED buffer in render
- **Fix**: Add assertions and early returns

### 3. Documentation
**Good**: All public APIs documented with doxygen
**Missing**:
- Algorithm explanations for complex math (curl noise)
- Performance characteristics notes
- Example usage code snippets

---

## Testing Improvements

### 1. Relaxed Assertions
**Location**: `test_polymetric_beat.cpp:130`, `test_rhythm_particles.cpp:143`
```cpp
CHECK(subdivision_count >= 0);  // Too relaxed
CHECK(fx.getActiveParticleCount() >= 0);  // Doesn't verify emission
```
**Fix**: Generate synthetic signals that trigger known behavior

### 2. Missing Edge Cases
- No tests for maximum particle capacity overflow
- No tests for zero-size grids
- No tests for configuration hot-swapping during playback
- **Fix**: Add boundary condition tests

### 3. Integration Test Coverage
- No tests with real audio streams
- No tests for memory leaks over time
- No performance benchmarks
- **Fix**: Add long-running stress tests

---

## Recommended Cleanup Priority

### High Priority (Do First)
1. **Replace hardcoded band indices** with enum/constants
2. **Fix config comparison** in `TipperBeats::setConfig()`
3. **Add null checks** in `RhythmParticles::render()`
4. **Extract magic numbers** to named constants

### Medium Priority (Nice to Have)
1. **Replace manual `new[]`/`delete[]`** with `fl::vector`
2. **Optimize particle culling** with swap-and-pop
3. **Improve RNG** to use FastLED's `random16()`
4. **Add performance benchmarks**

### Low Priority (Future Work)
1. **Optimize curl noise** calculations
2. **Add spatial indexing** for large grids
3. **Cache phase calculations**
4. **Add algorithm documentation**

---

## Refactoring Opportunities

### 1. Split Emitter Logic
**Current**: Emitters embedded in `RhythmParticles`
**Proposed**: Separate `ParticleEmitter` class
```cpp
class ParticleEmitter {
    ParticleEmitterConfig config;
    void emit(ParticlePool& pool, float energy);
};
```
**Benefits**: Better separation of concerns, testability

### 2. Template-ize SoA Storage
**Current**: Fixed particle storage structure
**Proposed**: Template-based SoA container
```cpp
template<int MaxParticles>
class SoAParticlePool {
    // Compile-time sized arrays
};
```
**Benefits**: No dynamic allocation, better optimization

### 3. Strategy Pattern for ODFs
**Current**: Switch statement for `OnsetDetectionFunction`
**Proposed**: Virtual function or function pointer
**Benefits**: Extensibility for custom ODFs

---

## Memory Profile (Approximate)

### Per-Instance Sizes
- `PolymetricAnalyzer`: ~100 bytes
- `RhythmParticles` (1000 particles): ~44KB
  - 11 float arrays: 4 bytes × 1000 × 11 = 44,000 bytes
- `TipperBeats`: ~45KB total (includes above)

### Heap Allocations
- **RhythmParticles**: 11 dynamic arrays (size depends on max_particles)
- **BeatDetector**: Several internal buffers (already existing)

**Recommendation**: Consider pre-allocated particle pool or template-based sizing

---

## API Stability Notes

### Stable (Safe to Use)
- All public methods in `TipperBeats`
- Configuration structures
- Callback signatures

### May Change in Future
- Internal SoA layout (if refactored to `fl::vector`)
- Emitter implementation details
- Curl noise calculation method

---

## Next Steps for Future Developers

1. **Read TASK.md** - Understand original requirements and design decisions
2. **Review test files** - Best examples of API usage
3. **Check this document** - Before making changes to understand impact
4. **Run benchmarks** - Before/after any performance optimization
5. **Update documentation** - Keep CLEAN.md current with changes

---

## API Stability & Breaking Changes

**IMPORTANT**: This is a **NEW FEATURE** with **NO EXISTING USERS** yet. We have complete freedom to make breaking changes before release.

### Before Release (NOW) - Free to Change
Since no user code exists yet, we can freely modify:
- ✅ Callback signatures - no one is using them yet
- ✅ `TipperBeatsConfig` structure - can restructure completely
- ✅ Public method names/signatures - rename as needed
- ✅ Class names - `TipperBeats` could become `PolymetricBeats` etc.
- ✅ **ALL public APIs** - this is the time to get them right

**API Improvements to Consider Now**:
1. Should `onOnsetBass/Mid/High` callbacks include frequency band info?
2. Is `TipperBeats` the right name, or too specific? Consider `PolymetricBeats` or `RhythmicParticles`
3. Should config structures use builder pattern instead of direct member access?
4. Is `processAudio()` taking raw samples the best API, or should it take a buffer struct?
5. Should particle emitters be externally configurable with custom types?

### Recommendations for Stable Release
Once released, these should become stable:
- 🔒 Callback signatures (`onBeat`, `onOnsetBass`, etc.)
- 🔒 Core config structures (`BeatDetectorConfig`, `RhythmParticlesConfig`)
- 🔒 Main class names (`TipperBeats`, `RhythmParticles`)
- 🔒 Public method signatures (`processAudio()`, `draw()`)

### Always Safe to Change
- ✅ Internal implementation details
- ✅ Private methods
- ✅ SoA storage mechanism
- ✅ Optimization strategies
- ✅ Test implementations
- ✅ Performance characteristics (as long as API stays same)

---

## Refactoring Summary

### Changes Required for Commit

**File Renames:**
1. `src/fx/2d/tipper_beats.h` → `src/fx/2d/polymetric_beats.h`
2. `src/fx/2d/tipper_beats.cpp` → `src/fx/2d/polymetric_beats.cpp`
3. `tests/test_tipper_beats.cpp` → `tests/test_polymetric_beats.cpp`

**Code Changes:**

1. **In `polymetric_beats.h`:**
   - Rename `TipperBeats` → `PolymetricBeats`
   - Rename `TipperBeatsConfig` → `PolymetricBeatsConfig`
   - Remove Tipper-specific defaults from `PolymetricBeatsConfig()` constructor
   - Add `PolymetricProfiles` namespace with profile presets:
     - `PolymetricProfiles::Tipper()` - Returns config for Tipper-style 7/8 overlay
     - Future: `Aphex()`, `Amon()`, etc.

2. **In `polymetric_beats.cpp`:**
   - Update all class references to `PolymetricBeats`
   - Update all config references to `PolymetricBeatsConfig`

3. **In `test_polymetric_beats.cpp`:**
   - Update all `TipperBeats` → `PolymetricBeats`
   - Update all `TipperBeatsConfig` → `PolymetricBeatsConfig`
   - Consider adding test using `PolymetricProfiles::Tipper()`

4. **Update documentation comments:**
   - Remove "Tipper-style" from class/file headers
   - Emphasize generic polymetric analysis
   - Mention Tipper as one example use case

### Rationale

This refactoring separates:
- **Algorithm** (generic polymetric beat detection) - `PolymetricBeats` class
- **Configuration** (artist-specific presets) - `PolymetricProfiles::Tipper()` function
- **Visualization** (particle system) - `RhythmParticles` class

The algorithm and visualization are already generic. Only the default configuration values were Tipper-specific, which is now properly expressed as a profile/preset rather than baked into the class name.

### Files That Don't Need Changes

✅ `src/fx/audio/polymetric_analyzer.{h,cpp}` - Already generic
✅ `src/fx/particles/rhythm_particles.{h,cpp}` - Already generic
✅ `tests/test_polymetric_beat.cpp` - Already tests generic analyzer
✅ `tests/test_rhythm_particles.cpp` - Already tests generic particles
✅ `src/fx/audio/beat_detector.{h,cpp}` - Generic beat detection
✅ `examples/BeatDetection/BeatDetection.ino` - Unchanged

---

## Contact / Questions

For questions about design decisions or implementation details:
- Review git history for commit messages with rationale
- Check test files for usage examples
- Refer to original TASK.md for requirements

**Good luck with cleanup! 🚀**
