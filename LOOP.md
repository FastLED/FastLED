# Chasing_Spirals_Q31_SIMD Refactoring - Agent Loop

## Mission: MAXIMUM READABILITY

Transform the 500-line Chasing_Spirals_Q31_SIMD function from brutal, repetitive SIMD code into a clean, maintainable example that serves as the high-water mark pattern for all Animartrix2 animations.

## Success Criteria - Readability Metrics

### Quantitative Goals
1. **Line count**: Reduce from 500 → 200-250 lines (50% reduction)
2. **Duplicate elimination**: Zero code blocks repeated more than once
3. **Cyclomatic complexity**: Main function < 15 branches
4. **Function length**: No single function > 100 lines
5. **Nesting depth**: Maximum 3 levels deep

### Qualitative Goals
1. **Self-documenting code**: Variable/function names explain intent without comments
2. **Single Responsibility**: Each helper does exactly one thing
3. **Visual scanability**: Can understand control flow in 30 seconds
4. **Cognitive load**: No mental juggling of more than 3 concepts simultaneously
5. **Pixel pipeline abstraction**: SIMD stride (4 pixels) decoupled from actual resolution

### Non-Goals
- **Performance optimization**: Not a priority (can recover later)
- **Cross-file refactoring**: Keep all helpers inline in chasing_spirals.hpp
- **Other variant cleanup**: Only Q31_SIMD (this is the pattern for others)

## Core Design Principles

### Pixel Pipeline Abstraction
**Problem**: Current code requires `total_pixels % 4 == 0`, tightly couples SIMD width to resolution.

**Solution approaches** (to be explored in Step 6):
- **Option A: Padding**: Always process `ceil(N/4)*4` pixels, ignore extras
- **Option B: Clipping**: Process full batches, handle remainder separately (current approach, but abstracted)
- **Option C: Overextend + Mask**: Process beyond bounds, mask writes

**Goal**: Main loop shouldn't care about SIMD width. Pipeline helpers handle batching.

### Helper Function Strategy
All helpers are **inline** within `chasing_spirals.hpp` in the `q31` namespace:
- **Prefix naming**: `simd4_` for 4-wide SIMD helpers, `pixel_` for per-pixel operations
- **Type safety**: Use strong types (FP, not raw i32) where possible
- **Const correctness**: All inputs const, no hidden state
- **Documentation**: One-line comment above each helper explaining what (not how)

## Test Strategy

**After every step:**
1. Run `bash test animartrix2 --cpp --verbose`
2. Verify output shows: `diff[0] + diff[1]` dominant, `diff[2+]` near zero
3. Check: "Q31 (non-SIMD) has fewer errors than Q31_SIMD" OR "same error count"

**Acceptance**: No visual regressions (LSB tolerance maintained)

**On failure**: Try fixing 2-3 times, then git reset to last commit

## Git Workflow

**Commit triggers** (after successful test):
- Major structural change (e.g., "Extract coordinate computation helpers")
- Milestone completion (e.g., "Complete RGB channel unification")
- Before risky changes (e.g., "Pre-refactor: pipeline abstraction")

**Commit message format**:
```
refactor(chasing_spirals): <what changed>

- <bullet point 1>
- <bullet point 2>

Lines: <before> → <after> (-X%)
Tests: ✅ animartrix2 accuracy maintained
```

**Reset policy**:
- Only after 2-3 failed fix attempts
- Preserve all documentation when resetting

## Agent Loop - Execution Steps

---

### Step 0: Baseline & Setup
**Agent**: General-purpose
**Task**:
- Run `bash test animartrix2 --cpp --verbose` to establish baseline
- Count current lines in Chasing_Spirals_Q31_SIMD (lines 244-530 in chasing_spirals.hpp)
- Save baseline metrics: line count, test output, diff histogram
- Create initial git commit: "refactor(chasing_spirals): baseline before Q31_SIMD cleanup"

**Success**: Tests pass, baseline documented

**Output**: `baseline_metrics.txt` with line count, test results

---

### Step 1: Relocate ChasingSpiralPixelLUT
**Agent**: General-purpose
**Task**:
- Move `struct ChasingSpiralPixelLUT` from `animartrix2_detail.hpp` (lines 85-92) to top of `chasing_spirals.hpp`
- Update the `using PixelLUT = ChasingSpiralPixelLUT;` alias to just use the local struct
- Keep the Engine member variable `mChasingSpiralLUT` but add a comment that it's Chasing_Spirals-specific
- Run tests to verify no breakage

**Success**: Tests pass, struct is now colocated with its usage

**Commit**: "refactor(chasing_spirals): move PixelLUT struct to chasing_spirals.hpp"

**Lines**: No change to Q31_SIMD function itself

---

### Step 2: Extract Common Setup Section
**Agent**: General-purpose
**Task**:
- Lines 248-320 are identical across all Chasing_Spirals variants
- Extract into inline helper: `inline void setupChasingSpiralFrame(Context& ctx, /* outputs */)`
- Helper should compute: timing, scaled constants, build PixelLUT, init fade LUT
- Replace duplicated setup in Q31_SIMD with single call
- Keep the extracted function **immediately above** Q31_SIMD for readability

**Success**: Tests pass, setup code appears once, Q31_SIMD starts at line ~260

**Commit**: "refactor(chasing_spirals): extract common frame setup"

**Lines**: ~500 → ~450 (-10%)

---

### Step 3: Extract Angle Computation Helper
**Agent**: General-purpose
**Task**:
- The pattern `(i64)(base + rad) * RAD_TO_A24 >> FRAC_BITS` appears 12 times (lines 344-347, 379-382, 412-415)
- Create inline helper:
  ```cpp
  // Convert s16x16 angle (radians) to A24 format for sincos32
  inline u32 radiansToA24(i32 base_s16x16, i32 offset_s16x16) {
      return static_cast<u32>((static_cast<i64>(base_s16x16 + offset_s16x16) * RAD_TO_A24) >> FP::FRAC_BITS);
  }
  ```
- Replace all 12 occurrences with calls to helper
- Use clear variable names at call sites

**Success**: Tests pass, angle conversion logic appears once

**Commit**: "refactor(chasing_spirals): extract angle computation helper"

**Lines**: ~450 → ~430 (-4%)

---

### Step 4: Extract SIMD Coordinate Computation Helper
**Agent**: General-purpose
**Task**:
- The pattern lines 358-365, 393-400, 426-433 computes `(nx, ny)` from sincos + distance
- Create helper:
  ```cpp
  // Compute Perlin coordinates from SIMD sincos results and distances (4 pixels)
  inline void simd4_computePerlinCoords(
      const u32 cos_arr[4], const u32 sin_arr[4],
      const i32 dist_arr[4], i32 lin_raw, i32 cx_raw, i32 cy_raw,
      i32 nx_out[4], i32 ny_out[4]) {
      // 4-way unrolled computation
  }
  ```
- Replace 3 duplicated blocks (red/green/blue) with calls
- Use descriptive variable names (nx_red, nx_green, nx_blue)

**Success**: Tests pass, coordinate computation appears once

**Commit**: "refactor(chasing_spirals): extract SIMD coordinate computation"

**Lines**: ~430 → ~390 (-9%)

---

### Step 5: Extract Clamp+Scale Helper
**Agent**: General-purpose
**Task**:
- The pattern `if (s < 0) s = 0; if (s > FP_ONE) s = FP_ONE; s *= 255;` appears 12 times
- Create inline helper:
  ```cpp
  // Clamp s16x16 value to [0, 1] and scale to [0, 255]
  inline i32 clampAndScale255(i32 raw_s16x16) {
      if (raw_s16x16 < 0) raw_s16x16 = 0;
      if (raw_s16x16 > FP_ONE) raw_s16x16 = FP_ONE;
      return raw_s16x16 * 255;
  }
  ```
- Replace all 12 occurrences (lines 371-374, 404-407, 437-440, etc.)

**Success**: Tests pass, clamp/scale logic appears once

**Commit**: "refactor(chasing_spirals): extract clamp+scale helper"

**Lines**: ~390 → ~360 (-8%)

---

### Step 6: Abstract Pixel Pipeline (CRITICAL REFACTOR)
**Agent**: General-purpose
**Task**:
- **Current**: Main loop `for (; i + 3 < total_pixels; i += 4)` followed by scalar fallback
- **Goal**: Abstract "process 4 pixels" into a pipeline that handles arbitrary pixel counts
- **Approach** (user wants padding/clipping abstraction):

Create pipeline helper:
```cpp
// Process N pixels through SIMD pipeline (4 at a time), handling remainder
// Uses padding: processes ceil(N/4)*4 pixels, discards extras via pixel_mask
inline void simd4_processPixelBatch(
    int start_pixel, int count, int total_pixels,
    const PixelLUT* lut, /* other shared params */,
    CRGB* leds) {

    // Process in batches of 4, even if count % 4 != 0
    for (int i = 0; i < count; i += 4) {
        // Determine which pixels in this batch are valid (mask)
        bool valid[4] = {
            start_pixel + i + 0 < total_pixels,
            start_pixel + i + 1 < total_pixels,
            start_pixel + i + 2 < total_pixels,
            start_pixel + i + 3 < total_pixels
        };

        // Process 4 pixels (SIMD path)
        // ... existing SIMD logic ...

        // Write only valid pixels
        for (int p = 0; p < 4; p++) {
            if (valid[p]) {
                leds[lut[start_pixel + i + p].pixel_idx] = /* result */;
            }
        }
    }
}
```

- **Main loop becomes**: `simd4_processPixelBatch(0, total_pixels, total_pixels, ...)`
- **No more % 4 assumption**: Pipeline handles arbitrary counts
- **Eliminate scalar fallback**: Pipeline processes remainder with masking

**Success**: Tests pass, main loop is ~5 lines, pixel count requirement removed

**Commit**: "refactor(chasing_spirals): abstract pixel pipeline with masking"

**Lines**: ~360 → ~280 (-22%)

**NOTE**: This is a risky change. Commit before attempting. If fails after 3 tries, reset.

---

### Step 7: Unify RGB Channel Processing
**Agent**: General-purpose
**Task**:
- Currently: 3 identical blocks for red, green, blue (lines 341-440)
- **Goal**: Single parameterized function for one channel, call 3 times

Create helper:
```cpp
// Process one color channel for 4 pixels (SIMD)
inline void simd4_processChannel(
    const i32 base_arr[4], const i32 dist_arr[4],
    i32 radial_offset, i32 linear_offset,
    /* shared params: fade_lut, perm, cx_raw, cy_raw */,
    i32 noise_out[4]) {

    // Compute angles (4-way)
    u32 angles[4];
    for (int i = 0; i < 4; i++) {
        angles[i] = radiansToA24(base_arr[i], radial_offset);
    }

    // SIMD sincos
    auto angles_vec = simd::load_u32_4(angles);
    auto sc = sincos32_simd(angles_vec);

    // Extract and compute coords
    u32 cos_arr[4], sin_arr[4];
    simd::store_u32_4(cos_arr, sc.cos_vals);
    simd::store_u32_4(sin_arr, sc.sin_vals);

    i32 nx[4], ny[4];
    simd4_computePerlinCoords(cos_arr, sin_arr, dist_arr,
                              linear_offset, cx_raw, cy_raw, nx, ny);

    // SIMD Perlin
    Perlin::pnoise2d_raw_simd4(nx, ny, fade_lut, perm, noise_out);

    // Clamp and scale
    for (int i = 0; i < 4; i++) {
        noise_out[i] = clampAndScale255(noise_out[i]);
    }
}
```

- Replace 3 blocks with 3 calls:
  ```cpp
  simd4_processChannel(base_arr, dist_arr, rad0_raw, lin0_raw, ..., s_r);
  simd4_processChannel(base_arr, dist_arr, rad1_raw, lin1_raw, ..., s_g);
  simd4_processChannel(base_arr, dist_arr, rad2_raw, lin2_raw, ..., s_b);
  ```

**Success**: Tests pass, RGB processing unified

**Commit**: "refactor(chasing_spirals): unify RGB channel processing"

**Lines**: ~280 → ~220 (-21%)

---

### Step 8: Extract Radial Filter Application
**Agent**: General-purpose
**Task**:
- Lines 443-469: Apply radial filter and clamp (repeated 4 times per batch)
- Create helper:
  ```cpp
  // Apply radial filter to noise value and clamp to [0, 255]
  inline i32 applyRadialFilter(i32 noise_255, i32 rf_raw, int shift_bits) {
      i32 result = static_cast<i32>((static_cast<i64>(noise_255) * rf_raw) >> shift_bits);
      if (result < 0) result = 0;
      if (result > 255) result = 255;
      return result;
  }
  ```
- Replace repeated blocks with:
  ```cpp
  i32 r = applyRadialFilter(s_r[i], lut[i].rf3.raw(), FP::FRAC_BITS * 2);
  i32 g = applyRadialFilter(s_g[i], lut[i].rf_half.raw(), FP::FRAC_BITS * 2);
  i32 b = applyRadialFilter(s_b[i], lut[i].rf_quarter.raw(), FP::FRAC_BITS * 2);
  ```

**Success**: Tests pass, radial filter logic appears once

**Commit**: "refactor(chasing_spirals): extract radial filter helper"

**Lines**: ~220 → ~200 (-9%)

---

### Step 9: Document and Polish
**Agent**: General-purpose
**Task**:
- Add section comments to main function explaining each stage:
  ```cpp
  // 1. Frame setup (timing, constants, LUTs)
  // 2. SIMD pixel pipeline (4-wide batches)
  // 3. RGB channel processing (3 channels × N/4 batches)
  // 4. Radial filter and output
  ```
- Add brief docstrings above each helper (what it does, not how)
- Ensure variable names are self-documenting (no `x`, `tmp`, use `angle_a24`, `noise_clamped`)
- Verify no magic numbers (all constants named)
- Check nesting depth (should be ≤3 levels)

**Success**: Code reads like prose, no head-scratching moments

**Commit**: "refactor(chasing_spirals): documentation and polish"

**Lines**: ~200 (±5 for comments)

---

### Step 10: Final Validation & Metrics
**Agent**: General-purpose
**Task**:
- Run `bash test animartrix2 --cpp --verbose` one final time
- Compare with baseline_metrics.txt:
  - Line count reduction
  - Test results identical
  - Diff histogram unchanged
- Generate final report:
  ```
  REFACTORING COMPLETE

  Lines: 500 → 200 (-60%)
  Duplicate blocks: 12 → 0
  Max function length: 500 → 80 lines
  Nesting depth: 4 → 2 levels

  Tests: ✅ All passing
  Accuracy: ✅ LSB tolerance maintained
  Visual regressions: ✅ None detected

  Readability improvements:
  - Pixel pipeline abstracted (no % 4 requirement)
  - RGB processing unified (single helper)
  - All magic constants named
  - Self-documenting variable names
  - Clear separation of concerns
  ```

**Commit**: "refactor(chasing_spirals): Q31_SIMD cleanup complete - 60% reduction"

---

## Recovery Procedures

### If Test Fails
1. Read error output carefully
2. Identify which refactoring step broke it
3. Attempt fix (max 3 tries):
   - Check for typos in helper calls
   - Verify parameter order matches
   - Ensure no off-by-one errors in loops
4. If unfixable after 3 tries: `git reset --hard <last-commit>`

### If Performance Degrades Significantly (>20%)
- **Don't panic**: This is expected (prioritized readability)
- Document the regression in commit message
- Continue with refactoring
- Performance recovery is a separate future effort

### If Documentation is Lost
- **Before resetting**: Check `git diff` to save comments
- When resetting: `git reset --soft` to keep changes, manually preserve docs
- Re-apply docs after fix

## Deliverables

1. **Cleaned chasing_spirals.hpp**: 200-250 lines for Q31_SIMD
2. **All tests passing**: No visual regressions
3. **Git history**: 8-10 clean commits showing progression
4. **Metrics report**: Quantitative improvement summary
5. **Pattern established**: This becomes the template for other animations

## Agent Handoff Protocol

Each step should output:
```
STEP X COMPLETE

Status: ✅ Success / ❌ Failed
Lines: <before> → <after> (-X%)
Tests: ✅ Pass / ❌ Fail
Commit: <sha> "<message>"

Next agent: Continue to Step X+1
```

On failure:
```
STEP X FAILED (Attempt Y/3)

Error: <description>
Fix attempted: <what was tried>

Next agent: Retry Step X / Reset and retry
```

---

## Start Command

To execute this loop:
```bash
# Agent will read this file and execute steps sequentially
# Each step runs, tests, commits, then hands off to next step
```

**First agent**: Begin with Step 0 (Baseline & Setup)
